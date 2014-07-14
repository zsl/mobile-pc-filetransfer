package com.zsl.file_transfer;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.UnknownHostException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

public class PcConnect {

    private Handler handler;
    private Socket socket;
    
    public PcConnect(Handler handler){
        this.handler = handler;
    }
    
    public void connect(String pcname, int port){
        
        Message msg = new Message();
        msg.what = PcinfoCmdFactory.CMD_CONNECTED;
        
        Bundle bundle = new Bundle();
        
        msg.setData(bundle);
        
        try {
            
            InetAddress addr = InetAddress.getByName(pcname);
            SocketAddress svrendpoint = new InetSocketAddress(addr, port);
            connect(svrendpoint);
            
            bundle.putBoolean("status", true);
            bundle.putString("pcname", pcname);
            
        } catch (UnknownHostException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", e.getMessage());
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", e.getMessage());
        }
        
        handler.sendMessage(msg);
    }
    
    
    public void connect(SocketAddress addr) throws IOException{
        socket = new Socket();
        socket.connect(addr);
    }

    public void ls(String dir) {
        // TODO Auto-generated method stub
        
        Message msg = new Message();
        msg.what = PcinfoCmdFactory.CMD_DIRS;
        
        Bundle bundle = new Bundle();
        
        msg.setData(bundle);
        
        try {
            
			JSONObject jsonLsCmd = new JSONObject();
			jsonLsCmd.put("type", "ls");
			jsonLsCmd.put("value", dir);
			
			byte[] cmddata = jsonLsCmd.toString().getBytes();
			
			String cmdheader = "len:";
			cmdheader += String.valueOf(cmddata.length);
			cmdheader += "\r\n";
			
			OutputStream outstream = new BufferedOutputStream(socket.getOutputStream());
			
			outstream.write(cmdheader.getBytes());
			outstream.write(cmddata);
			
			outstream.flush();
			
			InputStream reader = socket.getInputStream();
			
			int len = readLength(reader);
			
			if (len <= 0){
			    bundle.putBoolean("status", false);
			    bundle.putString("errmsg", String.format("%s: %d", "error header length", len));
			}
			else{
			    
			    byte[] recvdata = new byte[len];
			    
			    int readcount = 0;
			    
			    while (readcount < len){
    			    readcount += reader.read(recvdata, readcount, len - readcount);
			    }
			    
			    assert readcount == len;
			    
			    String jsonstr = new String(recvdata, "utf8");
			    
			    JSONObject json = new JSONObject(jsonstr);
			    String type = json.getString("type");
			    
			    assert type.equals("listdir");
			    
			    JSONArray dirs = json.getJSONArray("value");
			    
			    int dir_count = dirs.length();
			    String[] items = new String[dir_count];
			    String[] types = new String[dir_count];
			    
			    
			    for (int i = 0; i < dir_count; ++i){
			        items[i] = dirs.getJSONObject(i).getString("path");
			        types[i] = dirs.getJSONObject(i).getString("type");
			    }
			    
			    String parentDir = json.getString("parent");
			    
			    // 我们的socket是阻塞模式
			    assert parentDir.equals(dir);
			    
                bundle.putBoolean("status", true);
                bundle.putString("parent", parentDir);
                bundle.putStringArray("dirs", items);
                bundle.putStringArray("types", types);
                
			}
			
        } catch (JSONException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", String.format("JSONException, %s", e.getMessage()));
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", e.getMessage());
        }
        
        handler.sendMessage(msg);
    }
    
    private void download(final String filename){
        ;
    }

    private int readLength(InputStream reader) throws IOException{
        byte[] buf = new byte[255];
        
        int pos = 0;
        while (true){
            int count = reader.read(buf, pos, 1);
            if (buf[pos] == '\n'){
                pos += count;
                break;
            }
            
            pos += count;
        }
        
        String line = new String(buf, 0, pos, "utf-8");
        
        Pattern pattern = Pattern.compile("len:(\\d+)\\s*");
        Matcher match = pattern.matcher(line);
        
        if (match.matches()){
            String len = match.group(1);
            return Integer.valueOf(len).intValue();
        }
        else{
            return -1;
        }
    }
}
