from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer
from PIL import Image
import os

app = Flask(__name__)

# ONLY CLIP model for everything
clip_model = SentenceTransformer("sentence-transformers/clip-ViT-B-32")

UPLOAD_FOLDER = "/app/uploads"


@app.route("/embed/text", methods=["POST"])
def embed_text():
    """Embed text using CLIP model - works for both indexing and searching"""
    data = request.json
    text = data.get("text", "")

    # CLIP text encoding (no prefix needed)
    vector = clip_model.encode(
        text,
        convert_to_tensor=False,
        normalize_embeddings=True
    ).tolist()

    return jsonify({
        "vector": vector,
        "type": "clip",  # Single unified type
        "dimensions": len(vector)
    })


@app.route("/embed/image", methods=["POST"])
def embed_image():
    """Embed image using CLIP model"""
    data = request.json
    filename = data.get("filename", "")
    path = os.path.join(UPLOAD_FOLDER, filename)

    try:
        img = Image.open(path).convert("RGB")

        # CLIP image encoding
        vector = clip_model.encode(
            img,
            convert_to_tensor=False,
            normalize_embeddings=True
        ).tolist()

        return jsonify({
            "vector": vector,
            "type": "clip",  # Same type as text
            "dimensions": len(vector)
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5050)