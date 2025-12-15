from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer
from PIL import Image
import os

app = Flask(__name__)

# Multiple specialized models
clip_model = SentenceTransformer("sentence-transformers/clip-ViT-B-32")
text_model = SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")
ecommerce_model = SentenceTransformer("sentence-transformers/msmarco-distilbert-base-v4")

UPLOAD_FOLDER = "/app/uploads"


# ==================== TEXT EMBEDDING ====================

@app.route("/embed/text/clip", methods=["POST"])
def embed_text_clip():
    """CLIP text embedding - good for cross-modal (text-to-image) search"""
    data = request.json
    text = data.get("text", "")
    add_prefix = data.get("add_prefix", False)  # For search queries

    if add_prefix and len(text.split()) <= 3:
        text = f"a photo of {text}"

    vector = clip_model.encode(
        text,
        convert_to_tensor=False,
        normalize_embeddings=True
    ).tolist()

    return jsonify({
        "vector": vector,
        "model": "clip",
        "dimensions": len(vector)
    })


@app.route("/embed/text/semantic", methods=["POST"])
def embed_text_semantic():
    """Semantic text embedding - best for text-to-text matching"""
    data = request.json
    text = data.get("text", "")

    vector = text_model.encode(
        text,
        convert_to_tensor=False,
        normalize_embeddings=True
    ).tolist()

    return jsonify({
        "vector": vector,
        "model": "semantic",
        "dimensions": len(vector)
    })


@app.route("/embed/text/search", methods=["POST"])
def embed_text_search():
    """Search-optimized embedding - trained on query-document pairs"""
    data = request.json
    text = data.get("text", "")

    vector = ecommerce_model.encode(
        text,
        convert_to_tensor=False,
        normalize_embeddings=True
    ).tolist()

    return jsonify({
        "vector": vector,
        "model": "search",
        "dimensions": len(vector)
    })


@app.route("/embed/text/all", methods=["POST"])
def embed_text_all():
    """Generate all text embeddings at once - for product indexing"""
    data = request.json
    text = data.get("text", "")

    clip_vec = clip_model.encode(text, normalize_embeddings=True).tolist()
    semantic_vec = text_model.encode(text, normalize_embeddings=True).tolist()
    search_vec = ecommerce_model.encode(text, normalize_embeddings=True).tolist()

    return jsonify({
        "clip": {
            "vector": clip_vec,
            "dimensions": len(clip_vec)
        },
        "semantic": {
            "vector": semantic_vec,
            "dimensions": len(semantic_vec)
        },
        "search": {
            "vector": search_vec,
            "dimensions": len(search_vec)
        }
    })


# ==================== IMAGE EMBEDDING ====================

@app.route("/embed/image", methods=["POST"])
def embed_image():
    """CLIP image embedding"""
    data = request.json
    filename = data.get("filename", "")
    path = os.path.join(UPLOAD_FOLDER, filename)

    try:
        img = Image.open(path).convert("RGB")

        vector = clip_model.encode(
            img,
            convert_to_tensor=False,
            normalize_embeddings=True
        ).tolist()

        return jsonify({
            "vector": vector,
            "model": "clip",
            "dimensions": len(vector)
        })
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "healthy", "models_loaded": 3})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5050)