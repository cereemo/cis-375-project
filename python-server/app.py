from flask import Flask, send_from_directory, request, jsonify
import os

FRONTEND_DIR = os.path.join(os.path.dirname(__file__), "..", "frontend")

app = Flask(__name__, static_folder=None)

# ---------- Serve frontend ----------
@app.get("/")
def home():
    return send_from_directory(FRONTEND_DIR, "index.html")

@app.get("/<path:path>")
def frontend_files(path):
    return send_from_directory(FRONTEND_DIR, path)

PRODUCTS = [
    {"id": "laptop", "name": "HP Laptop", "price": 299.99, "category": "Electronics"},
    {"id": "earbuds", "name": "Wired USB-C Earbuds", "price": 19.99, "category": "Electronics"},
    {"id": "calculator", "name": "Casio Calculator", "price": 29.99, "category": "School"},
    {"id": "hoodie", "name": "Gray Hoodie", "price": 49.99, "category": "Clothing"}
]

@app.get("/api/products")
def api_products():
    q = (request.args.get("q") or "").lower()
    if not q:
        return jsonify(PRODUCTS)

    return jsonify([
        p for p in PRODUCTS
        if q in p["name"].lower() or q in p["category"].lower()
    ])

@app.post("/api/login")
def api_login():
    data = request.get_json(silent=True) or {}
    if data.get("email") == "test@example.com" and data.get("password") == "password":
        return jsonify({"ok": True})
    return jsonify({"ok": False}), 401

if __name__ == "__main__":
    app.run(debug=True)
