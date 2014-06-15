package com.zsl.file_transfer;

public interface IoHandler {
	void handle_read();
	void handle_write();
	void handle_connect();
}
