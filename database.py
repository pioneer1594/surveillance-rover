"""
db_seed.py — Smart GuardX Database Seeder
==========================================
Run this ONCE after setting up PostgreSQL to create your first admin user and car.

Usage:
    python db_seed.py

Edit the DEFAULT_* constants below to choose your login credentials.
"""

import sys
import bcrypt
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

# ── Import models from main.py ──────────────────────────────────────────────
sys.path.insert(0, ".")
from backend import Base, User, Car, DATABASE_URL, datetime

# ── Your desired credentials ─────────────────────────────────────────────────
# Change these before running!
DEFAULT_USERNAME     = "admin"
DEFAULT_PASSWORD     = "SecurePass@123"   # Will be hashed with bcrypt
DEFAULT_CAR_NAME     = "Rover 1"
DEFAULT_CAR_WIFI_PW  = "123456"  # This is the 3rd login factor

# ─────────────────────────────────────────────────────────────────────────────

def seed():
    print(f"[Seed] Connecting to: {DATABASE_URL}")
    engine = create_engine(DATABASE_URL)

    # Create tables if they don't exist yet                                                                                                                                         
    Base.metadata.create_all(bind=engine)
    print("[Seed] Tables verified/created.")

    Session = sessionmaker(bind=engine)
    db = Session()

    try:
        # Check if user already exists
        existing = db.query(User).filter(User.username == DEFAULT_USERNAME).first()
        if existing:
            print(f"[Seed] User '{DEFAULT_USERNAME}' already exists — skipping creation.")
            print(f"[Seed] Existing cars: {[c.car_name for c in existing.cars]}")
            return

        # Hash the password with bcrypt (auto-generates a salt)                                                                                                                                                 

        hashed = bcrypt.hashpw(
            DEFAULT_PASSWORD.encode("utf-8"),                                                                                                                   
            bcrypt.gensalt(rounds=12)
        ).decode("utf-8")

        # Create the user
        user = User(
            username=DEFAULT_USERNAME,
            password_hash=hashed
        )
        db.add(user)
        db.flush()  # Get the user.id without committing                                                                        

        # Create the car linked to that user
        car = Car(                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
            car_name=DEFAULT_CAR_NAME,
            wifi_password=DEFAULT_CAR_WIFI_PW,
            owner_id=user.id
        )
        db.add(car)

        #Create a seesion for the car 
        #session = Session(
        #    user_id=user.id,
        #   car_id=car.id,
        #  login_time=datetime.utcnow(),
        # status="active"
        #    )

        #db.add(session)
        db.commit()
        #db.refresh(session)
        print()
        print("=" * 50)
        print("  [Seed] SUCCESS — User and Car created!")
        print("=" * 50)
        print(f"  Username      : {DEFAULT_USERNAME}")
        print(f"  Password      : {DEFAULT_PASSWORD}")
        print(f"  Car Name      : {DEFAULT_CAR_NAME}")
        print(f"  Car Wi-Fi PW  : {DEFAULT_CAR_WIFI_PW}")
        print("=" * 50)
        print()
        print("  Use these 3 credentials to log into the dashboard.")
        print()

    except Exception as e:
        db.rollback()
        print(f"[Seed] ERROR: {e}")
        raise
    finally:
        db.close()


if __name__ == "__main__":
    seed()