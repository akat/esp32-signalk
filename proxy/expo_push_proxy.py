#!/usr/bin/env python3
"""
Expo Push Notification HTTP Proxy
Receives HTTP requests from ESP32 and forwards them to Expo API via HTTPS

Install dependencies:
    pip install flask requests

Run:
    python expo_push_proxy.py

The proxy will listen on port 8080 (HTTP, no SSL)
ESP32 sends: POST http://YOUR_SERVER:8080/push
Proxy forwards to: POST https://exp.host/--/api/v2/push/send
"""

from flask import Flask, request, jsonify
import requests
import logging
from datetime import datetime

app = Flask(__name__)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

# Expo API endpoint
EXPO_API_URL = 'https://exp.host/--/api/v2/push/send'

# Statistics
stats = {
    'total_requests': 0,
    'successful': 0,
    'failed': 0,
    'last_request': None
}


@app.route('/push', methods=['POST'])
def push_notification():
    """Forward push notification to Expo API"""
    stats['total_requests'] += 1
    stats['last_request'] = datetime.now().isoformat()

    try:
        # Get JSON payload from ESP32
        payload = request.get_json()

        if not payload:
            logging.warning("Empty payload received")
            stats['failed'] += 1
            return jsonify({'error': 'Empty payload'}), 400

        # Log the request
        token_preview = payload.get('to', 'unknown')[:30] if payload.get('to') else 'unknown'
        logging.info(f"Forwarding push to token: {token_preview}...")

        # Forward to Expo API
        response = requests.post(
            EXPO_API_URL,
            json=payload,
            headers={'Content-Type': 'application/json'},
            timeout=10
        )

        # Log response
        if response.status_code == 200:
            stats['successful'] += 1
            logging.info(f"Success: {response.status_code}")
        else:
            stats['failed'] += 1
            logging.warning(f"Expo API error: {response.status_code} - {response.text}")

        # Return Expo's response to ESP32
        return response.text, response.status_code

    except requests.exceptions.Timeout:
        stats['failed'] += 1
        logging.error("Timeout connecting to Expo API")
        return jsonify({'error': 'Timeout'}), 504

    except requests.exceptions.RequestException as e:
        stats['failed'] += 1
        logging.error(f"Request error: {str(e)}")
        return jsonify({'error': str(e)}), 502

    except Exception as e:
        stats['failed'] += 1
        logging.error(f"Unexpected error: {str(e)}")
        return jsonify({'error': 'Internal server error'}), 500


@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint"""
    return jsonify({
        'status': 'healthy',
        'service': 'expo-push-proxy',
        'stats': stats
    })


@app.route('/stats', methods=['GET'])
def get_stats():
    """Statistics endpoint"""
    return jsonify(stats)


if __name__ == '__main__':
    logging.info("=" * 50)
    logging.info("Expo Push Notification HTTP Proxy")
    logging.info("=" * 50)
    logging.info("Starting server on http://0.0.0.0:8080")
    logging.info("Endpoints:")
    logging.info("  POST /push   - Forward push notification")
    logging.info("  GET  /health - Health check")
    logging.info("  GET  /stats  - View statistics")
    logging.info("=" * 50)

    # Run Flask server
    # For production, use: gunicorn -w 4 -b 0.0.0.0:8080 expo_push_proxy:app
    app.run(host='0.0.0.0', port=8080, debug=False)
