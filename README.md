<p align="left">
   <img src="https://img.shields.io/badge/Python-3.9-blue" alt="Python Version">
</p>

# SNMP-Monitor
A project to monitor the status of the switch, in my case it was old switch HP ProCurve 2512, but you can take my project and modify it to suit your needs. This project uses SNMP (Simple Network Management Protocol). The script collects statistics about ports, transmitted traffic, errors and other parameters of the device. The data is displayed in the web-interface for easy network monitoring.

# Requirements
To run the project, you need:

1. Python version 3.9 or higher.
2. Installed dependencies from requirements.txt:
3. Flask - for web server.
4. A network configured to communicate with the device via SNMP (for example, correct IP addresses and ports).
5. An SNMP-enabled switch or router configured with a community string, such as public.

### Project Structure
```
SNMP-Monitor/
├── app/
│   ├── __init__.py            # Initializes the Flask application
│   ├── snmp_monitor.py        # Main script to gather SNMP data
│   ├── templates/
│   │   └── index.html         # HTML template for the dashboard
│   └── static/
│       └── style.css          # CSS file for styling the webpage
├── requirements.txt           # Project dependencies (Flask, pysnmp, etc.)
└── README.md                  # Documentation for the project
```
## Installation
1. **Clone the repository:**
 ```
git clone https://github.com/TerminalExploit/SNMP-Monitor.git
cd SNMP-Monitor
```
2. **Install dependencies:**
```
pip install -r requirements.txt
```
3. Configure SNMP on your switch (since the project can be used with different switches, search the internet for instructions or ask ChatGPT :D).
4. Start the Flask server:
```
python app/__init__.py
```
5. Go to http://localhost:5000 in your browser.

### Note: Compatibility with Other Devices
This project is not limited to the HP ProCurve 2512 switch. It can be adapted to work with any network devices that support SNMP. Since SNMP is a universal protocol used by many modern network devices, including switches, routers, and servers from various manufacturers (e.g., Cisco, Juniper, MikroTik, Netgear, D-Link), this project allows you to monitor these devices as well.

To make the project work with other devices, you may need to:

Enable SNMP on the target device and configure the community string (e.g., public).
Adjust OIDs in the project to match the SNMP object identifiers for the device you are using (OID values can differ between devices).
