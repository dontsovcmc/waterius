# -*- coding: utf-8 -*-
__author__ = 'dontsov'

import asyncore
import socket
import logging


class TcpServer(asyncore.dispatcher):
    def __init__(self, host, port, handle):
        asyncore.dispatcher.__init__(self)
        self.logger = logging.getLogger('Server')
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind((host, port))
        self.address = self.socket.getsockname()
        self.logger.debug('binding to %s', self.address)
        self.listen(5)
        self._handle = handle

    def handle_accept(self):
        # Called when a client connects to our socket
        client_info = self.accept()
        if client_info is not None:
            self.logger.debug('handle_accept() -> %s', client_info[1])
            ClientHandler(client_info[0], client_info[1], self._handle)

    def loop(self):
        asyncore.loop()


class ClientHandler(asyncore.dispatcher):
    def __init__(self, sock, address, handle):
        asyncore.dispatcher.__init__(self, sock)
        self.logger = logging.getLogger('Client ' + str(address))
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
        self.logger.debug('handle_write() -> (%d) "%s"', sent, data[:sent].rstrip())

    def handle_read(self):
        data = self.recv(1024)
        self.logger.debug('handle_read() -> (%d)', len(data))
        try:
            self._handle(data)
            #self.data_to_write.insert(0, data)
        except Exception, err:
            self.logger.error('handle_read(): %s' % err)

    def handle_close(self):
        self.logger.debug('handle_close()')
        self.close()