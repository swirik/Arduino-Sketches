import cv2
import mediapipe as mp
import numpy as np
import serial
import time

# ======== CONFIG ========
PORT = "COM9"          # Set your ESP32 serial port (e.g., "COM3" on Windows, "/dev/ttyUSB0" on Linux)
BAUD = 115200
SMOOTH_ALPHA = 0.5     # 0..1, higher = more smoothing (try 0.45..0.65)
MAX_STEP_DEG = 10     # max degrees per frame per finger (lower = smoother)
PRINT_ANGLES = True    # print angles in this terminal for debugging

# Per-finger mechanical limits (deg)
# Thumb, Index, Middle, Ring, Pinky (PCA9685 channels 0..4)
ANGLES_MIN = np.array([0,  0,  0,  0,  0], dtype=float)
ANGLES_MAX = np.array([180, 180, 180, 180, 180], dtype=float)

# If a finger moves opposite direction, flip it by setting 1
# Commonly thumb may need flipping: [1,0,0,0,0]
INVERT = np.array([1, 0, 1, 0, 1], dtype=int)

# ======== Hand model ========
mp_hands = mp.solutions.hands
mp_draw  = mp.solutions.drawing_utils
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1,
    model_complexity=1,
    min_detection_confidence=0.6,
    min_tracking_confidence=0.6
)

# Indices for landmarks (MediaPipe)
# Finger joints (tip, pip, mcp) per finger
FINGERS = {
    "thumb":  (4, 3, 2),     # tip, ip, mcp (thumb special; not used in dist method)
    "index":  (8, 6, 5),     # tip, pip, mcp
    "middle": (12,10,9),
    "ring":   (16,14,13),
    "pinky":  (20,18,17),
}
ORDER = ["thumb", "index", "middle", "ring", "pinky"]

def angle_between(a, b, c):
    """(kept for reference) Return angle at b (degrees) formed by vectors ba and bc."""
    ba = a - b
    bc = c - b
    nba = ba / (np.linalg.norm(ba) + 1e-6)
    nbc = bc / (np.linalg.norm(bc) + 1e-6)
    cosang = np.clip(np.dot(nba, nbc), -1.0, 1.0)
    return np.degrees(np.arccos(cosang))

# --- continuous, distance-based curl (prevents binary snaps) ---
def finger_curl_01(landmarks, img_w, img_h):
    """
    Continuous curl in [0..1] for thumb,index,middle,ring,pinky.
    Uses normalized distances instead of angles to avoid jumps.
    0=open (far), 1=closed (near).
    """
    lm = np.array([[p.x * img_w, p.y * img_h, p.z] for p in landmarks], dtype=float)

    def norm_len(a, b, eps=1e-6):
        return np.linalg.norm(lm[a][:2] - lm[b][:2]) + eps

    curls = []

    # ---- THUMB: tip(4) vs index MCP(5), normalized by palm (wrist(0)↔middle MCP(9))
    palm_norm = norm_len(0, 9)
    d_thumb = norm_len(4, 5) / palm_norm  # larger when open
    # map: far ≥0.90 => 0, near ≤0.35 => 1 (tune if needed)
    thumb = np.interp(d_thumb, [0.90, 0.35], [0.0, 1.0])
    curls.append(float(np.clip(thumb, 0, 1)))

    # ---- OTHER FINGERS: tip vs mcp, normalized by (mcp↔pip) = finger segment
    spec = {
        "index":  (8, 5, 6),
        "middle": (12, 9, 10),
        "ring":   (16, 13, 14),
        "pinky":  (20, 17, 18),
    }
    for name in ["index", "middle", "ring", "pinky"]:
        tip, mcp, pip = spec[name]
        seg = norm_len(mcp, pip)              # finger scale
        d = norm_len(tip, mcp) / seg          # larger when open
        # tuneable window: far ≥3.5 => 0, near ≤1.8 => 1
        curl = np.interp(d, [3.5, 1.8], [0.0, 1.0])
        curls.append(float(np.clip(curl, 0, 1)))

    return np.array(curls, dtype=float)

def map_curl_to_angles(curls):
    # Optionally flip
    curls = np.where(INVERT == 1, 1.0 - curls, curls)
    # 0..1 -> per-finger angle
    return ANGLES_MIN + (ANGLES_MAX - ANGLES_MIN) * curls

def limit_step(prev, new, step):
    """Limit per-frame change to avoid sudden jumps."""
    delta = new - prev
    delta = np.clip(delta, -step, step)
    return prev + delta

# ---- Serial init (retry until ESP32 is ready) ----
ser = None
for _ in range(20):
    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.02)
        time.sleep(1.5)  # allow ESP32 to reset
        break
    except Exception:
        time.sleep(0.2)
if ser is None:
    print(f"Could not open serial port {PORT}. Edit PORT and try again.")
    raise SystemExit

# ---- Camera ----
cap = cv2.VideoCapture(0)  # change index if needed
# Make capture more stable/consistent
cap.set(cv2.CAP_PROP_FPS, 30)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

prev_angles = np.array([90, 90, 90, 90, 90], dtype=float)

print("Press 'q' to quit.")
while True:
    ok, frame = cap.read()
    if not ok:
        break
    frame = cv2.flip(frame, 1)  # mirror for convenience
    h, w = frame.shape[:2]

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    res = hands.process(rgb)

    if res.multi_hand_landmarks:
        lm = res.multi_hand_landmarks[0].landmark
        curls = finger_curl_01(lm, w, h)         # 5 values 0..1
        target = map_curl_to_angles(curls)       # 5 angles (deg)

        # Smoothing (EMA)
        ema = SMOOTH_ALPHA * target + (1 - SMOOTH_ALPHA) * prev_angles
        # Rate limit to prevent snapping
        angles = limit_step(prev_angles, ema, MAX_STEP_DEG)
        prev_angles = angles

        # Overlay numbers
        for i, name in enumerate(ORDER):
            cv2.putText(frame, f"{name[:2]}:{int(angles[i]):3d}",
                        (10, 30 + 22*i), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255), 2)

        # Send to ESP32
        msg = "{:.0f},{:.0f},{:.0f},{:.0f},{:.0f}\n".format(*angles)
        try:
            ser.write(msg.encode('ascii'))
        except Exception:
            pass

        if PRINT_ANGLES:
            print("PC→ESP32 angles:", ",".join(f"{int(a)}" for a in angles))

        # draw landmarks (optional)
        mp_draw.draw_landmarks(frame, res.multi_hand_landmarks[0], mp_hands.HAND_CONNECTIONS)
    else:
        # No hand detected: hold previous angles (do nothing)
        pass

    cv2.imshow("Hand → Servo", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
ser.close()
cv2.destroyAllWindows()
