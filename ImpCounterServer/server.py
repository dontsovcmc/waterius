# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import asyncore
import socket
from logger import log


class TcpServer(asyncore.dispatcher):
    def __init__(self, host, port, handle):
        asyncore.dispatcher.__init__(self)
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((host, port))
        self.address = self.socket.getsockname()
        log.debug('binding to %s', self.address)
        self.listen(5)
        self._handle = handle

    def handle_accept(self):
        # Called when a client connects to our socket
        client_info = self.accept()
        if client_info is not None:
            log.debug('handle_accept() -> %s', client_info[1])
            ClientHandler(client_info[0], client_info[1], self._handle)

    def loop(self):
        asyncore.loop()


class ClientHandler(asyncore.dispatcher):
    def __init__(self, sock, address, handle):
        asyncore.dispatcher.__init__(self, sock)
        self.data_to_write = []
        self._handle = handle

    def writable(self):
        return bool(self.data_to_write)

    def handle_write(self):
        data = self.data_to_write.pop()
        sent = self.send(data[:1024])
        if sent < len(data):
            remaining = data[sent:]
            self.data.to_write.append(remaining)
        log.debug('handle_write() -> (%d) "%s"', sent, data[:sent].rstrip())

    def handle_read(self):
        data = self.recv(1024)
        log.debug('handle_read() -> (%d)', len(data))
        try:
            self._handle(data)
            #self.data_to_write.insert(0, data)
        except Exception, err:
            log.error('handle_read(): %s' % err)
        self.close()

    def handle_close(self):
        log.debug('handle_close()')
        self.close()