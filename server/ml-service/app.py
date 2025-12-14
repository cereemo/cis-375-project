from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer
from PIL import Image
import os

app = Flask(__name__)

model = SentenceTransformer('clip-ViT-B-32')

UPLOAD_FOLDER = '/app/uploads'

@app.route('/embed/text', methods=['POST'])
def embed_text():
    data = request.json
    text = data.get('text', '')
    # Embed and convert to standard list
    vector = model.encode(text).tolist()
    return jsonify({'vector': vector})

@app.route('/embed/image', methods=['POST'])
def embed_image():
    data = request.json
    # C++ sends the filename it just saved
    filename = data.get('filename', '')
    path = os.path.join(UPLOAD_FOLDER, filename)

    try:
        img = Image.open(path)
        vector = model.encode(img).tolist()
        return jsonify({'vector': vector})
    except Exception as e:
        return jsonify({'error': str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5050)