package com.zsl.file_transfer;

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.UnknownHostException;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.provider.OpenableColumns;
import android.util.Log;

public class PcConnect {

    private Handler handler;
    private Socket socket;
    
    private String mPcname;
    private int mPort;
    
    private Map<String, String> mDownloadMap = new HashMap<String, String>();
    
    public PcConnect(Handler handler){
        this.handler = handler;
    }
    
    public void connect(String pcname, int port){
        
        mPcname = pcname;
        mPort = port;
        
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
			
			writeCmd(socket.getOutputStream(), jsonLsCmd);
			
			InputStream reader = socket.getInputStream();
			
			String jsonstr = readResponse(reader);
			
			    
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
                
			
        } catch (JSONException e) {
            Log.e(this.getClass().getSimpleName(), "JSONException", e);
            
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", String.format("JSONException, %s", e.getMessage()));
        } catch (IOException e) {
            Log.e(this.getClass().getSimpleName(), "IOException", e);
            
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", e.getMessage());
        } catch (ErrCmdLengthException e) {
            Log.e(this.getClass().getSimpleName(), "IOException", e);
            
            bundle.putBoolean("status", false);
            bundle.putString("errmsg", String.format("JSONException, %s", e.getMessage()));
        }
        
        handler.sendMessage(msg);
    }
    
    public void download(final String fileFullPath, final String savePath){
        try {
            
            InetAddress addr = InetAddress.getByName(mPcname);
            SocketAddress svrendpoint = new InetSocketAddress(addr, mPort);
            
            Socket sockDownload = new Socket();
            
            sockDownload.connect(svrendpoint);
            
            // 连接成功后，就开始下载吧
			JSONObject jsonLsCmd = new JSONObject();
			jsonLsCmd.put("type", "download");
			jsonLsCmd.put("value", fileFullPath);
			
			writeCmd(sockDownload.getOutputStream(),jsonLsCmd);
			
			InputStream reader = sockDownload.getInputStream();
			String jsonstr = readResponse(reader);
			
			JSONObject json = new JSONObject(jsonstr);
			
			String type = json.getString("type");
			
			assert type.equals("download");
			
			String fileName = json.getString("value").replace('\\', '/');
			String sendFileName = fileFullPath.replace('\\', '/');
			
			assert fileName == sendFileName;
			
			long fileLen = json.getLong("filelen");
			byte[] buf = new byte[4096];
			
			OutputStream out = new FileOutputStream(savePath);
			
			long leftLen = fileLen;
			while (leftLen > 0){
			    int readLen = reader.read(buf, 0, buf.length);
			    out.write(buf, 0, readLen);
			    
			    leftLen -= readLen;
			}
			
			out.flush();
			out.close();
			// 最后关闭套接字
			sockDownload.close();
            
        } catch (UnknownHostException e) {
            Log.e(this.getClass().getSimpleName(), "download: UnknownHostException", e);
        } catch (IOException e) {
            Log.e(this.getClass().getSimpleName(), "download: IOException", e);
        } catch (JSONException e) {
            Log.e(this.getClass().getSimpleName(), "download: JSONException", e);
        } catch (ErrCmdLengthException e) {
            Log.e(this.getClass().getSimpleName(), "download: ErrCmdLengthException", e);
        }
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
    
    private void writeCmd(OutputStream out, JSONObject json) throws IOException{
        
		byte[] cmddata = json.toString().getBytes();
		
		String cmdheader = "len:";
		cmdheader += String.valueOf(cmddata.length);
		cmdheader += "\r\n";
		
		out.write(cmdheader.getBytes());
		out.write(cmddata);
		
		out.flush();
    }
    
    private String readResponse(InputStream in) throws ErrCmdLengthException, IOException{
        
        int len = readLength(in);
        
        if (len <= 0){
            throw new ErrCmdLengthException("err length");
        }
        else{
            
            byte[] recvdata = new byte[len];
            
            int readcount = 0;
            
            while (readcount < len){
                readcount += in.read(recvdata, readcount, len - readcount);
            }
            
            assert readcount == len;
            
            return new String(recvdata, "utf8");
        }
    }
    
    class ErrCmdLengthException extends Exception{

        private static final long serialVersionUID = 1L;

        public ErrCmdLengthException(String msg){
            super(msg);
        }
    }
}
