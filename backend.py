import time
import math
import os
from pathlib import Path
import bcrypt
import cv2
import numpy as np
import requests
from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import StreamingResponse
from fastapi.staticfiles import StaticFiles
from datetime import datetime
from pydantic import BaseModel
from sqlalchemy import (
    Column, Integer, String, ForeignKey, DateTime, Text,
    create_engine
)
from sqlalchemy.orm import declarative_base, relationship, sessionmaker
from datetime import datetime

app = FastAPI()

# CORS Middleware (Browser error များ မတက်စေရန်)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/")
async def serve_ui():
    return FileResponse("Combineindex.html")

# ================= Security Setup =================
def get_password_hash(password: str) -> str:
    salt = bcrypt.gensalt()
    return bcrypt.hashpw(password.encode('utf-8'), salt).decode('utf-8')

def verify_password(plain_password: str, hashed_password: str) -> bool:
    return bcrypt.checkpw(plain_password.encode('utf-8'), hashed_password.encode('utf-8'))

# ================= Database Setup =================
DATABASE_URL = "postgresql://postgres:shein123@localhost/SSR"

engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

# ================= ORM Models =================

class User(Base):
    __tablename__ = "users"

    id            = Column(Integer, primary_key=True, index=True)
    username      = Column(String(64), unique=True, nullable=False, index=True)
    password_hash = Column(String(256), nullable=False)

    cars = relationship("Car", back_populates="owner", cascade="all, delete-orphan")


class Car(Base):
    __tablename__ = "cars"

    id           = Column(Integer, primary_key=True, index=True)
    car_name     = Column(String(128), nullable=False)
    wifi_password = Column(String(256), nullable=False)
    owner_id     = Column(Integer, ForeignKey("users.id"), nullable=False)

    owner  = relationship("User", back_populates="cars")
    videos = relationship("Video", back_populates="car", cascade="all, delete-orphan")


class Video(Base):
    __tablename__ = "videos"

    id          = Column(Integer, primary_key=True, index=True)
    filename    = Column(String(256), nullable=False)
    filepath    = Column(Text, nullable=False)
    recorded_at = Column(DateTime, default=datetime.utcnow)
    car_id      = Column(Integer, ForeignKey("cars.id"), nullable=False)

    car = relationship("Car", back_populates="videos")

# ================= Lifespan Setup =================
@asynccontextmanager
async def lifespan(app: FastAPI):
    try:
        Base.metadata.create_all(bind=engine)
        print("[DB] Tables created / verified OK.")
    except Exception as e:
        print(f"[DB] WARNING: Could not connect to database: {e}")
    yield

# ================= App Setup =================
app = FastAPI(
    title="Smart GuardX API Server",
    lifespan=lifespan
)

RECORDS_DIR = Path(__file__).parent / "records"
RECORDS_DIR.mkdir(exist_ok=True)

app.mount("/records", StaticFiles(directory=str(RECORDS_DIR)), name="records")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ================= Pydantic Schemas =================

class RegisterPayload(BaseModel):
    username: str
    password: str

class LoginPayload(BaseModel):
    username: str
    password: str
    car_wifi: str

class ConnectionConfig(BaseModel):
    mode: str  # "demo", "webcam", "esp32"
    ip: str = ""

class MovePayload(BaseModel):
    direction: str

class PanPayload(BaseModel):
    angle: int

class SystemState:
    def __init__(self):
        self.mode = "esp32"  # "webcam", "esp32", "demo"
        self.esp32_ip = "192.168.4.1"
        self.connected = True
        self.servo_angle = 90
        self.current_direction = "stop"
        self.logs = []

state = SystemState()

def add_log(message: str):
    timestamp = time.strftime("%H:%M:%S")
    state.logs.append({"time": timestamp, "message": message})
    if len(state.logs) > 50:
        state.logs.pop(0)

add_log("System initialized. Mode: WEBCAM.")

def encode_jpeg(frame):
    ok, jpeg = cv2.imencode(".jpg", frame)
    if not ok:
        raise RuntimeError("Failed to encode frame as JPEG")
    return jpeg.tobytes()

def test_esp32_control(ip: str) -> bool:
    """Ping the ESP32 control API health endpoint before streaming."""
    res = requests.get(f"http://{ip}/", timeout=1.5)
    res.raise_for_status()
    return True

def get_demo_frame(t_sec):
    frame = np.zeros((480, 640, 3), dtype=np.uint8)
    frame[:] = [30, 20, 15]

    grid_size = 40
    for x in range(0, 640, grid_size):
        cv2.line(frame, (x, 0), (x, 480), (50, 35, 25), 1)
    for y in range(0, 480, grid_size):
        cv2.line(frame, (0, y), (640, y), (50, 35, 25), 1)

    center = (320, 240)
    for r in [80, 160, 240]:
        cv2.circle(frame, center, r, (70, 50, 35), 1)

    sweep_angle = t_sec * 2.0
    sx = int(320 + 240 * math.cos(sweep_angle))
    sy = int(240 + 240 * math.sin(sweep_angle))
    cv2.line(frame, center, (sx, sy), (150, 100, 30), 1)

    t1_x = int(320 + 140 * math.cos(t_sec * 0.4))
    t1_y = int(240 + 90 * math.sin(t_sec * 0.3))
    cv2.rectangle(frame, (t1_x - 30, t1_y - 45), (t1_x + 30, t1_y + 45), (0, 0, 220), 2)
    cv2.putText(frame, "TARGET: INTRUDER (94%)", (t1_x - 30, t1_y - 52), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 0, 220), 1, cv2.LINE_AA)

    t2_x = int(320 + 160 * math.cos(t_sec * 0.2 + 2.0))
    t2_y = int(240 + 120 * math.sin(t_sec * 0.25 + 1.0))
    cv2.rectangle(frame, (t2_x - 25, t2_y - 25), (t2_x + 25, t2_y + 25), (0, 200, 0), 2)
    cv2.putText(frame, "TARGET: PET (98%)", (t2_x - 25, t2_y - 32), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 200, 0), 1, cv2.LINE_AA)

    cv2.putText(frame, "SYS_STATUS: ACTIVE", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)
    cv2.putText(frame, f"SERVO PAN: {state.servo_angle} DEG", (20, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)
    cv2.putText(frame, f"DRIVE DIRECTION: {state.current_direction.upper()}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)

    cv2.putText(frame, "STREAM_MODE: SIMULATION", (400, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 180, 255), 1, cv2.LINE_AA)
    cur_time = time.strftime("%Y-%m-%d %H:%M:%S")
    cv2.putText(frame, cur_time, (400, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 180, 255), 1, cv2.LINE_AA)

    cv2.putText(frame, "SMART GUARDX SECURE VIEW", (210, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 200, 255), 1, cv2.LINE_AA)

    if int(t_sec * 2) % 2 == 0:
        cv2.circle(frame, (600, 25), 6, (0, 0, 255), -1)
        cv2.putText(frame, "REC", (560, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1, cv2.LINE_AA)

    return encode_jpeg(frame)

def process_webcam_frame(frame, t_sec):
    frame = cv2.resize(frame, (640, 480))

    cv2.putText(frame, "SYS_STATUS: ACTIVE", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)
    cv2.putText(frame, f"SERVO PAN: {state.servo_angle} DEG", (20, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)
    cv2.putText(frame, f"DRIVE DIRECTION: {state.current_direction.upper()}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)

    cv2.putText(frame, "STREAM_MODE: LOCAL WEBCAM", (410, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)
    cur_time = time.strftime("%Y-%m-%d %H:%M:%S")
    cv2.putText(frame, cur_time, (410, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)

    cv2.putText(frame, "SMART GUARDX WEBCAM VIEW", (210, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1, cv2.LINE_AA)

    if int(t_sec * 2) % 2 == 0:
        cv2.circle(frame, (600, 25), 6, (0, 0, 255), -1)
        cv2.putText(frame, "REC", (560, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1, cv2.LINE_AA)

    return encode_jpeg(frame)

ESP32_CAPTURE_URL = "http://192.168.4.1/capture"
is_recording = False
video_writer = None

def start_recording_stream(output_filename):
    global is_recording, s
    
    # Save folder မရှိရင် ဆောက်မည်
    os.makedirs("recordings", exist_ok=True)
    filepath = os.path.join("recordings", output_filename)
    
    # Video Writer Config (320x240 resolution, 10 FPS)
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video_writer = cv2.VideoWriter(filepath, fourcc, 10.0, (320, 240))
    
    is_recording = True
    print(f"Started Recording: {filepath}")

    while is_recording:
        try:
            # ESP32 ထံမှ HTTP Capture Frame ဆွဲယူခြင်း
            img_resp = requests.get(ESP32_CAPTURE_URL, timeout=2)
            img_arr = np.array(bytearray(img_resp.content), dtype=np.uint8)
            frame = cv2.imdecode(img_arr, -1)

            if frame is not None:
                # Video File ထဲသို့ Frame ရေးထည့်ခြင်း
                video_writer.write(frame)
        except Exception as e:
            print(f"Frame Capture Error: {e}")
            break

    if video_writer:
        video_writer.release()
    print("Recording Stopped.")

def stop_recording_stream():
    global is_recording
    is_recording = False

def process_esp32_frame(jpg_bytes, t_sec):
    try:
        frame = cv2.imdecode(np.frombuffer(jpg_bytes, np.uint8), cv2.IMREAD_COLOR)
        if frame is None:
            return jpg_bytes
        frame = cv2.resize(frame, (640, 480))

        cv2.putText(frame, "SYS_STATUS: CONNECTED", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)
        cv2.putText(frame, f"SERVO PAN: {state.servo_angle} DEG", (20, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)
        cv2.putText(frame, f"DRIVE DIRECTION: {state.current_direction.upper()}", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)

        cv2.putText(frame, f"ESP32-CAM: {state.esp32_ip}", (420, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)
        cur_time = time.strftime("%Y-%m-%d %H:%M:%S")
        cv2.putText(frame, cur_time, (420, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)

        cv2.putText(frame, "SMART GUARDX REMOTE VIEW", (210, 450), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1, cv2.LINE_AA)

        if int(t_sec * 2) % 2 == 0:
            cv2.circle(frame, (600, 25), 6, (0, 0, 255), -1)
            cv2.putText(frame, "REC", (560, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 255), 1, cv2.LINE_AA)

        return encode_jpeg(frame)
    except Exception as e:
        add_log(f"Warning: ESP32 frame overlay failed: {str(e)}")
        return jpg_bytes

def event_generator():
    cap = None
    last_mode = None

    try:
        while True:
            current_mode = state.mode
            t_sec = time.time()

            if current_mode != last_mode:
                add_log(f"Stream source changed to {current_mode.upper()}")
                if cap is not None:
                    cap.release()
                    cap = None
                last_mode = current_mode

            try:
                if current_mode == "demo":
                    jpg = get_demo_frame(t_sec)
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + jpg + b'\r\n')
                    time.sleep(0.05)

                elif current_mode == "webcam":
                    if cap is None:
                        cap = cv2.VideoCapture(0)
                        if not cap.isOpened():
                            add_log("Error: Webcam could not be initialized. Defaulting to Demo.")
                            state.mode = "demo"
                            cap = None
                            continue

                    ret, frame = cap.read()
                    if not ret:
                        time.sleep(0.01)
                        continue
                    jpg = process_webcam_frame(frame, t_sec)
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + jpg + b'\r\n')
                    time.sleep(0.033)

                elif current_mode == "esp32":
                    if not state.esp32_ip:
                        add_log("Error: ESP32 IP is empty. Defaulting to Demo.")
                        state.mode = "demo"
                        continue

                    # Poll ESP32 single capture URL
                    capture_url = f"http://{state.esp32_ip}/capture"
                    try:
                        res = requests.get(capture_url, timeout=2.0)
                        if res.status_code == 200:
                            state.connected = True
                            processed_jpg = process_esp32_frame(res.content, t_sec)
                            yield (b'--frame\r\n'
                                   b'Content-Type: image/jpeg\r\n\r\n' + processed_jpg + b'\r\n')
                        else:
                            state.connected = False
                            state.mode = "demo"
                    except Exception as e:
                        add_log(f"Error fetching frame from ESP32: {str(e)}")
                        state.connected = False
                        state.mode = "demo"
                    time.sleep(0.05)

            except Exception as e:
                add_log(f"Stream generation exception: {str(e)}")
                time.sleep(1.0)
    finally:
        if cap is not None:
            cap.release()

@app.get("/api/status")
def get_status():
    return {
        "mode": state.mode,
        "esp32_ip": state.esp32_ip,
        "connected": state.connected,
        "servo_angle": state.servo_angle,
        "current_direction": state.current_direction,
        "logs": state.logs
    }

@app.post("/api/connect")
def connect(config: ConnectionConfig):
    mode = config.mode.lower()
    if mode not in ["demo", "webcam", "esp32"]:
        raise HTTPException(status_code=400, detail="Invalid mode selected")

    state.mode = mode
    if mode == "esp32":
        if not config.ip:
            raise HTTPException(status_code=400, detail="IP address required for ESP32 mode")
        state.esp32_ip = config.ip
        add_log(f"Attempting control connection to ESP32: {config.ip}")
        try:
            test_esp32_control(config.ip)
            state.connected = True
            add_log("ESP32 control API reachable.")
        except Exception as e:
            add_log(f"Warning: ESP32 control API unreachable: {str(e)}. Stream will still be attempted.")
            state.connected = False
    elif mode == "webcam":
        state.esp32_ip = ""
        state.connected = False
        add_log("Switched to Local Webcam mode.")
    else:
        state.esp32_ip = ""
        state.connected = False
        add_log("Switched to Simulation/Demo mode.")

    return get_status()

@app.post("/api/control/move")
def move(payload: MovePayload):
    direction = payload.direction.lower()
    if direction not in ["forward", "backward", "left", "right", "stop"]:
        raise HTTPException(status_code=400, detail="Invalid direction")

    state.current_direction = direction
    add_log(f"Motor direction set to: {direction.upper()}")

    if state.mode == "esp32" and state.esp32_ip:
        command_map = {
            "forward": "F",
            "backward": "B",
            "left": "L",
            "right": "R",
            "stop": "S"
        }
        command = command_map[direction]
        esp32_url = f"http://{state.esp32_ip}/action?go={command}"

        try:
            res = requests.get(esp32_url, timeout=1.5)
            res.raise_for_status()
            return {"status": "relayed", "esp32_response": res.text}
        except Exception as e:
            add_log(f"Failed to send movement to ESP32: {str(e)}")
            return {"status": "failed", "error": str(e)}

    return {"status": "simulated", "direction": direction}

@app.post("/api/control/pan")
def pan(payload: PanPayload):
    angle = payload.angle
    if not (0 <= angle <= 180):
        raise HTTPException(status_code=400, detail="Angle must be between 0 and 180")

    state.servo_angle = angle
    add_log(f"Camera Pan set to: {angle} degrees")

    if state.mode == "esp32" and state.esp32_ip:
        esp32_url = f"http://{state.esp32_ip}/pan?angle={angle}"
        try:
            res = requests.get(esp32_url, timeout=1.5)
            res.raise_for_status()
            return {"status": "relayed", "esp32_response": res.text}
        except Exception as e:
            add_log(f"Failed to send pan command to ESP32: {str(e)}")
            return {"status": "failed", "error": str(e)}

    return {"status": "simulated", "angle": angle}

@app.get("/api/stream")
def stream():
    return StreamingResponse(event_generator(), media_type="multipart/x-mixed-replace; boundary=frame")

@app.post("/api/register")
def register(payload: RegisterPayload):
    db = SessionLocal()
    try:
        existing_user = db.query(User).filter(User.username == payload.username).first()
        if existing_user:
            raise HTTPException(status_code=400, detail="Username already exists")

        new_user = User(
            username=payload.username,
            password_hash=get_password_hash(payload.password)
        )
        db.add(new_user)
        db.commit()
        return {"success": True, "message": "Account created successfully."}
    finally:
        db.close()

@app.post("/api/login")
def login(payload: LoginPayload):
    db = SessionLocal()
    try:
        user = db.query(User).filter(User.username == payload.username).first()
        if not user:
            raise HTTPException(status_code=401, detail="ACCESS DENIED: User not found.")

        if not verify_password(payload.password, user.password_hash):
            raise HTTPException(status_code=401, detail="ACCESS DENIED: Incorrect password.")

        car = db.query(Car).filter(Car.wifi_password == payload.car_wifi).first()
        if not car:
            raise HTTPException(status_code=401, detail="ACCESS DENIED: Incorrect Car Wi-Fi Password.")

        add_log(f"Successful login: user='{user.username}' connected to car='{car.car_name}'")
        return {
            "success": True,
            "username": user.username,
            "car_connected": car.car_name
        }
    finally:
        db.close()

@app.get("/api/videos")
def list_videos():
    try:
        files = sorted(
            [f for f in os.listdir(RECORDS_DIR) if f.lower().endswith(".webm")],
            reverse=True
        )
        add_log(f"Video list requested. Found {len(files)} recording(s).")
        return {"videos": files}
    except Exception as e:
        add_log(f"Error listing videos: {str(e)}")
        raise HTTPException(status_code=500, detail=f"Could not read records directory: {str(e)}")
