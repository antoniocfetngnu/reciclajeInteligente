import torch
import torchvision.transforms.functional as F
from torchvision.models.detection.faster_rcnn import FastRCNNPredictor
from torchvision.models.detection import fasterrcnn_resnet50_fpn, FasterRCNN_ResNet50_FPN_Weights
from PIL import Image
import io
import base64
from flask import Flask, request, jsonify, render_template
from flask_socketio import SocketIO
import numpy as np
import cv2
import gc  # Garbage collection for clearing memory
import time
from threading import Thread
import sqlite3
from datetime import datetime, timedelta



class ObjectDetector:
    def __init__(self, model_path, num_classes=3):
        self.device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        print(f"Using device: {self.device}")
        
        if torch.cuda.is_available():
            print(f"CUDA is available: {torch.version.cuda}")
            print(f"Device Name: {torch.cuda.get_device_name(0)}")
        else:
            print("CUDA is not available. Using CPU.")
        
        # Load model architecture
        self.model = self.get_object_detection_model(num_classes)
        
        # Load saved weights
        checkpoint = torch.load(model_path, map_location=self.device)
        self.model.load_state_dict(checkpoint['model_state_dict'])
        
        self.model.to(self.device)
        self.model.eval()

    def get_object_detection_model(self, num_classes):
        model = fasterrcnn_resnet50_fpn(weights=FasterRCNN_ResNet50_FPN_Weights.COCO_V1)
        
        # Replace the classifier head
        in_features = model.roi_heads.box_predictor.cls_score.in_features
        model.roi_heads.box_predictor = FastRCNNPredictor(in_features, num_classes)
        
        return model

    def predict(self, image):
        # Prepare image
        image_tensor = F.to_tensor(image).to(self.device)
        
        # Prediction
        with torch.no_grad():
            predictions = self.model([image_tensor])
        
        # Process predictions
        best_prediction = None
        highest_score = 0
        
        for idx, box in enumerate(predictions[0]['boxes']):
            score = predictions[0]['scores'][idx]
            label = predictions[0]['labels'][idx]
            
            if score > highest_score:
                highest_score = score
                best_prediction = {
                    "label": label.item(),
                    "score": score.item(),
                    "box": box.tolist()
                }
        
        # Classify result
        if best_prediction is None or highest_score < 0.5:
            prediccion = "sin accion"
        else:
            if best_prediction["label"] == 1:
                prediccion = "botella"
            elif best_prediction["label"] == 2:
                prediccion = "papel"
            else:
                prediccion = "sin accion"
        
        # Clear Torch cache to free memory
        torch.cuda.empty_cache()
        gc.collect()  # Run garbage collection
        
        return {
            "prediction": prediccion,
            "details": best_prediction
        }

DATABASE = 'db_basurero.db'

# Función para obtener la conexión
def get_db_connection():
    conn = sqlite3.connect(DATABASE)
    conn.row_factory = sqlite3.Row
    return conn

# Crear la tabla si no existe (solo una vez)
def create_table():
    conn = get_db_connection()
    conn.execute('''
        CREATE TABLE IF NOT EXISTS items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            objeto TEXT NOT NULL,
            datetime TEXT NOT NULL,
            url_imagen TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()
# Flask Application
app = Flask(__name__)
create_table()

socketio = SocketIO(app)

# Variable para enviar a los clientes





@app.route('/Nivel/<int:valor>', methods=['GET'])

def send_nivel(valor): 
        socketio.emit('nivel', {'value': valor})
        return f"Evento 'nivel' emitido con valor: {valor}", 200


@app.route('/Estado/<int:valor>', methods=['GET'])
def set_estado(valor):
    # Convertir valor a booleano (aceptar "true" o "false", insensible a mayúsculas)
    socketio.emit('estado', {'value': valor})
    # Confirmar al cliente que el estado fue procesado
    return f"Estado recibido: {valor}", 200


# Initialize model 
detector = ObjectDetector('fasterrcnn_model1.pth')

# Web Interface Route
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/graficas')
def graficas():
    conn = get_db_connection()

    # Total de cada objeto
    total_objetos = conn.execute('''
        SELECT objeto, COUNT(*) as total
        FROM items
        GROUP BY objeto
    ''').fetchall()
    total_objetos = [{'objeto': row['objeto'], 'total': row['total']} for row in total_objetos]

    # Última semana
    hoy = datetime.now().date()
    hace_una_semana = hoy - timedelta(days=6)
    por_dia = conn.execute('''
        SELECT objeto, 
               substr(datetime, 1, 4) || '-' || substr(datetime, 5, 2) || '-' || substr(datetime, 7, 2) as dia,
               COUNT(*) as total
        FROM items
        WHERE dia BETWEEN ? AND ?
        GROUP BY dia, objeto
    ''', (hace_una_semana, hoy)).fetchall()
    por_dia = [{'objeto': row['objeto'], 'dia': row['dia'], 'total': row['total']} for row in por_dia]

    # Día actual
    por_hora = conn.execute('''
        SELECT objeto, 
               substr(datetime, 10, 2) as hora,
               COUNT(*) as total
        FROM items
        WHERE substr(datetime, 1, 8) = ?
        GROUP BY hora, objeto
    ''', (hoy.strftime('%Y%m%d'),)).fetchall()
    por_hora = [{'objeto': row['objeto'], 'hora': row['hora'], 'total': row['total']} for row in por_hora]

    # Por mes
    por_mes = conn.execute('''
        SELECT objeto, 
               substr(datetime, 1, 4) || '-' || substr(datetime, 5, 2) as mes,
               COUNT(*) as total
        FROM items
        GROUP BY mes, objeto
    ''').fetchall()
    por_mes = [{'objeto': row['objeto'], 'mes': row['mes'], 'total': row['total']} for row in por_mes]

    conn.close()

    return render_template('graficas.html', 
                           total_objetos=total_objetos,
                           por_dia=por_dia,
                           por_hora=por_hora,
                           por_mes=por_mes)



# ESP32-CAM Route
@app.route('/predict_esp32', methods=['POST'])
def predict_from_esp32():
    if request.data is None or len(request.data) == 0:
        return jsonify({"error": "No image data"}), 400

    try:
        # Convertir imagen a formato PIL
        image = Image.open(io.BytesIO(request.data))
        
        # Guardar la imagen localmente
        from datetime import datetime
        import os
        save_directory = "static/esp32_images"
        os.makedirs(save_directory, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        file_path = os.path.join(save_directory, f"image_{timestamp}.jpg")
        image.save(file_path)

        # Detectar objetos
        result = detector.predict(image)
        objeto = result['prediction']
        url_imagen = f"/static/esp32_images/image_{timestamp}.jpg"

        # Insertar en la base de datos
        conn = get_db_connection()
        conn.execute(
            'INSERT INTO items (objeto, datetime, url_imagen) VALUES (?, ?, ?)',
            (objeto, timestamp, url_imagen)
        )
        conn.commit()
        conn.close()

        # Emitir eventos a los sockets
        socketio.emit('estado', {'value': 2})
        socketio.emit('objeto', {'value': objeto})
        socketio.emit('imagen', {'value': url_imagen})

        # Responder al ESP32
        esp32_response = {
            "prediction": result['prediction'],
            "confidence": result['details']['score'] if result['details'] else 0,
            "class": result['details']['label'] if result['details'] else 0
        }
        return jsonify(esp32_response)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    # app.run(host='0.0.0.0', port=4001, debug=True)
    # Inicia un hilo para actualizar la variable periódicamente
    socketio.run(app, host='0.0.0.0', port=4001, debug=True)
