from flask import Flask, render_template
from snmp_monitor import get_snmp_data

app = Flask(__name__)

@app.route('/')
def index():
    data = get_snmp_data('192.168.1.1')
    return render_template('index.html', data=data)

if __name__ == '__main__':
    app.run(debug=True)
