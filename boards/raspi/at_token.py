#!/usr/bin/env python3

from ctypes import *
import base64
import json
import datetime
import sys
import jwt
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec

def hw_sign(msg):
    cryptolib = cdll.LoadLibrary('../../.build/libcryptoauth.so')

    status = cryptolib.atcab_init(cryptolib.cfg_ateccx08a_i2c_default)
    assert 0 == status

    digest = create_string_buffer(32)
    status = cryptolib.atcab_sha(len(msg), c_char_p(msg), byref(digest))
    assert 0 == status

    signature = create_string_buffer(64)
    status = cryptolib.atcab_sign(0, byref(digest), byref(signature))
    assert 0 == status
    return signature.raw

def man_jwt(claims):

    header = '{"typ":"JWT","alg":"ES256"}'
    
    for k, v in claims.items():
        if type(v) is datetime.datetime:
            claims[k] = int(v.timestamp())
    
    payload = json.dumps(claims)
    #print('payload', payload)
    
    token = base64.urlsafe_b64encode(header.encode('ascii')).replace(b'=', b'') + b'.'
    #print('token', token)

    token = token + base64.urlsafe_b64encode(payload.encode('ascii')).replace(b'=',b'')
    #print('token', token)

    signature = hw_sign(token)

    token = token + b'.' + base64.urlsafe_b64encode(signature).replace(b'=',b'')
    return token

class HwEcAlgorithm(jwt.algorithms.Algorithm):
    def __init__(self):
        self.hash_alg = hashes.SHA256

    def prepare_key(self, key):
        return key
    
    def sign(self, msg, key):
        return hw_sign(msg)

    def verify(self, msg, key, sig):
        try:
            der_sig = jwt.utils.raw_to_der_signature(sig, key.curve)
        except ValueError:
            return False

        try:
            key.verify(der_sig, msg, ec.ECDSA(self.hash_alg()))
            return True
        except InvalidSignature:
            return False


def jwt_with_hw_alg():
    #print('from pyjwt')
    inst = jwt.PyJWT(algorithms=[])
    inst.register_algorithm('ES256', HwEcAlgorithm())
    return inst
    

if __name__ == '__main__':

    if len(sys.argv) < 2:
        print('Invalid number of arguments provided')
        exit(1)

    if len(sys.argv) == 3:
        mins = int(sys.argv[2])
    else:
        mins = 60

    claims = { 'iat': datetime.datetime.utcnow(),
        'exp': datetime.datetime.utcnow() + datetime.timedelta(minutes=mins),
        'aud': sys.argv[1] }

    token = man_jwt(claims).decode('ascii') + '\n\n'
    print(token)

