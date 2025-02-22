from pysnmp.hlapi import *
import json


def get_snmp_data(ip):
    oid = '1.3.6.1.2.1.2.2.1.10.1'
    data = snmp_get('public', ip, oid)
    return data


def snmp_get(community, ip, oid):
    iterator = getCmd(SnmpEngine(),
                      CommunityData(community),
                      UdpTransportTarget((ip, 161)),
                      ContextData(),
                      ObjectType(ObjectIdentity(oid)))
    
    errorIndication, errorStatus, errorIndex, varBinds = next(iterator)
    
    if errorIndication:
        return {'error': str(errorIndication)}
    elif errorStatus:
        return {'error': '%s at %s' % (errorStatus.prettyPrint(), errorIndex)}
    else:
        result = {}
        for varBind in varBinds:
            result['oid'] = str(varBind[0])
            result['value'] = str(varBind[1])
        return result
