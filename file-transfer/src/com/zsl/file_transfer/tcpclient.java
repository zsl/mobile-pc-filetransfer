package com.zsl.file_transfer;

import java.io.IOException;
import java.net.SocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;

public class tcpclient implements IoHandler{

    Selector selector;
    SocketChannel channel;
    
    ByteBuffer[] sendbuf = new ByteBuffer[2];
    
    tcpclient(Selector selector){
        this.selector = selector;
    }
    
    boolean is_connect(){
        return channel != null && channel.isConnected();
    }
    
    void connect(SocketAddress addr){
        try {
            channel = SocketChannel.open();
            channel.configureBlocking(false);
            
            if (!channel.connect(addr)){
                channel.register(selector, SelectionKey.OP_CONNECT, this);
            }
            else{
                handle_connect();
            }
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }
    
    void ls(String dir){
        try {
            
            String data = new String();
            
			JSONObject jsonLsCmd = new JSONObject();
			jsonLsCmd.put("type", "ls");
			jsonLsCmd.put("value", dir);
			
			byte[] cmddata = jsonLsCmd.toString().getBytes();
			
			String cmdheader = "len:";
			cmdheader += String.valueOf(cmddata.length);
			cmdheader += "\r\n";
			
			sendbuf[0] = ByteBuffer.wrap(cmdheader.getBytes());
			sendbuf[1] = ByteBuffer.wrap(cmddata);
			
            channel.register(selector, SelectionKey.OP_WRITE, this);
        } catch (ClosedChannelException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        } catch (JSONException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    @Override
    public void handle_read() {
        // TODO Auto-generated method stub
        
    }

    @Override
    public void handle_write() {
        // TODO Auto-generated method stub
        
    }

    @Override
    public void handle_connect() {
        // TODO Auto-generated method stub
        
    }
}
