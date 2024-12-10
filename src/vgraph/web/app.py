from flask import Flask, render_template, jsonify, request
import requests

app = Flask(__name__)
VGRAPH_API = "http://127.0.0.1:8080"

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/node', methods=['POST'])
def add_node():
    try:
        response = requests.post(f"{VGRAPH_API}/node", json=request.json)
        return jsonify(response.json()), response.status_code
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/edge', methods=['POST'])
def add_edge():
    try:
        response = requests.post(f"{VGRAPH_API}/edge", json=request.json)
        return jsonify(response.json()), response.status_code
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/shortest_path/<int:from_id>/<int:to_id>')
def get_shortest_path(from_id, to_id):
    try:
        response = requests.get(f"{VGRAPH_API}/shortest_path/{from_id}/{to_id}")
        return jsonify(response.json()), response.status_code
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/graph')
def get_graph():
    try:
        response = requests.get(f"{VGRAPH_API}/graph")
        return jsonify(response.json()), response.status_code
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True) 