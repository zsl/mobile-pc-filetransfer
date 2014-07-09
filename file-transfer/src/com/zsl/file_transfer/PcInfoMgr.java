package com.zsl.file_transfer;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.MulticastSocket;
import java.net.SocketAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.*;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;

public class PcInfoMgr implements IoHandler {
	private DatagramChannel udpchannel;
	private InetSocketAddress endpoint = new InetSocketAddress("225.0.0.1", 10000);
	
	private Map<String, tcpclient> connections = new HashMap<String, tcpclient>();

	private Selector selector;
	
	private String waitPcName;
	
	PcInfoMgr(){

	}

	void connect(final String pcname){
	    new Thread(){
	        public void run(){
	            try {
	                
	                waitPcName = pcname;
	                
	                InetAddress addr = InetAddress.getByName(pcname);
	                SocketAddress svrendpoint = new InetSocketAddress(addr, 10000);
	                
	                tcpclient client = new tcpclient(selector);
	                
	                client.connect(svrendpoint);
	                
                    connections.put(pcname, client);
                    
	            } catch (IOException e) {
	                // TODO Auto-generated catch block
	                e.printStackTrace();
	            }

	        }
	    }.start();
	}
	
	void ls(String pcname, String dir)
	{
	    if (connections.containsKey(pcname)){
	        connections.get(pcname).ls(dir);
	    }
	}
	
	void run()
	{
		new Thread(){
			public void run(){
				
				try {
					udpchannel = DatagramChannel.open();
					
					selector = Selector.open();
					
					JSONObject pcInfoCmd = new JSONObject();
					pcInfoCmd.put("type", "pcinfo");
					
					System.out.println(pcInfoCmd.toString());
								
					int writecount = udpchannel.send(ByteBuffer.wrap(pcInfoCmd.toString().getBytes()), endpoint);

				    System.out.println(String.format("send count: %d", writecount));
					
					udpchannel.configureBlocking(false);
					udpchannel.register(selector, SelectionKey.OP_READ, this);
				} catch (SocketException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} catch (IOException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} catch (JSONException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} 
				
				while (true) {
					try {
						
						int keycount = selector.select();
						
						if (keycount > 0){
							Set<SelectionKey> selectedKeys = selector.selectedKeys();
							Iterator<SelectionKey> it = selectedKeys.iterator();
							
							while (it.hasNext()){
								SelectionKey selectionKey = it.next();
								IoHandler handler = (IoHandler) selectionKey.attachment();
								
								if (selectionKey.isReadable()){
									handler.handle_read();
								}
								else if (selectionKey.isWritable()){
									handler.handle_write();
								}
								else if (selectionKey.isConnectable()){
									handler.handle_connect();
								}
								
								it.remove();
							}

						}
						
					} catch (IOException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
				
			}
		}
		.start();
	}
	
	@Override
	public void handle_read() {
		// 这里只会接收到utp的包
		ByteBuffer target = ByteBuffer.allocate(1024);
		try {
			
			SocketAddress pcAddr = udpchannel.receive(target);
		    // 先把数据转换为string吧
			Charset charset = Charset.forName("utf-8");
			CharsetDecoder decoder = charset.newDecoder();
			CharBuffer buffer = decoder.decode(target);
			
			String json = buffer.toString();
			
			JSONObject jsonObj = new JSONObject(json);
			String cmdType = jsonObj.getString("type");
			
			if (cmdType.equals("pcname")){
				String pcname = jsonObj.getString("value");
				System.out.println("new pc: " + pcname);
			}
			
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (JSONException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
	}

	@Override
	public void handle_write() {
		// TODO Auto-generated method stub
		System.out.println("handle write");
	}

	@Override
	public void handle_connect() {
		// TODO Auto-generated method stub
		System.out.println("handle_connect");
	}
}
