package com.zsl.file_transfer;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.util.Log;

public class NetworkService extends Service {
    
    // 发送广播的Action
    public static String NETWORK_ACTION = "com.zsl.file_transfer.NETWORK_ACTION";
    
    private static int DOWN_FILE_COUNT = 3;
    
    private PcConnect connect;
    private Map<String, PcConnect> connectMap = new HashMap<String, PcConnect>();
    private Future<PcConnect> futureConnect; // 用于接收线程的结果
    private PcinfoCmdFactory cmdfactory;
    private ExecutorService pool;
    private ExecutorService downloadPool;
    
    private Handler handler = new Handler(new DirCmdHandlerCallback());
    
    // 线程工厂类，设置线程为后台线程
    private class DeamonThreadFactory implements ThreadFactory {

        @Override
        public Thread newThread(Runnable r) {
            // TODO Auto-generated method stub
            Thread thread = new Thread(r);
            thread.setDaemon(true);
            
            return thread;
        }
        
    }
    
    private Binder binder = new NetworkBinder();
    
    
    public NetworkService() {
    }

    public boolean isConnectComputer(String computerName){
        return connectMap.get(computerName) != null;
    }
    
    public void connect(String computerName, int port){
        futureConnect = pool.submit(cmdfactory.newConnectCmd(computerName, port));
        Log.i(this.getClass().getSimpleName(), "proc connect");
    }
    
    public void ls(String computerName, String dir){
        PcConnect con = connectMap.get(computerName);
        if (con == null){
            // 报告错误，还没有连接
        }
        else{
            pool.submit(cmdfactory.newLsCmd(con, dir));
        }
        
        Log.i(this.getClass().getSimpleName(), String.format("proc ls: %s/%s", computerName, dir));
    }
    
    public void download(String computerName, String fileFullPath, String savePath){
        PcConnect con = connectMap.get(computerName);
        if (con == null){
            // 报告错误，还没有连接
        }
        else{
            downloadPool.submit(cmdfactory.newDownloadCmd(con, fileFullPath, savePath));
        }
        
        Log.i(this.getClass().getSimpleName(), String.format("proc download: %s/%s", computerName, fileFullPath));
    }
    
    @Override
    public void onCreate(){
        // 创建线程池
        pool = Executors.newSingleThreadExecutor(new DeamonThreadFactory());
        downloadPool = Executors.newFixedThreadPool(DOWN_FILE_COUNT);
        
        cmdfactory = new PcinfoCmdFactory(handler);
    }
    
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // We want this service to continue running until it is explicitly
        // stopped, so return sticky.
        return START_STICKY;
    }

    
    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        return binder;
    }
    
    // 我们自己的Binder
    public class NetworkBinder extends Binder{
        public NetworkService getService(){
            return NetworkService.this;
        }
    }
    
    
    // 线程的handler
    private class DirCmdHandlerCallback implements Handler.Callback{

        @Override
        public boolean handleMessage(Message msg) {
            // TODO Auto-generated method stub
            switch (msg.what){
                case PcinfoCmdFactory.CMD_CONNECTED:
                {
                    Bundle bundle = msg.getData();
                    boolean status = bundle.getBoolean("status");
                    
                    if (status){
                        try {
                            
                            connect = futureConnect.get();
                            String pcname = bundle.getString("pcname");
                            connectMap.put(pcname, connect);
                            
                            // 如果链接成功，那么我们就开始列出目录
                            pool.submit(cmdfactory.newLsCmd(connect, ""));
                            
                        } catch (InterruptedException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        } catch (ExecutionException e) {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        }
                    }
                    else{
                        String errmsg = bundle.getString("errmsg");
                        System.out.println(errmsg);
                        // 发送连接失败广播
                        Intent intent = new Intent(NETWORK_ACTION);
                        intent.putExtra("action", msg.what);
                        intent.putExtra("data", bundle);
                        
                        sendBroadcast(intent);
                    }
                    
                    break;
                }
                default:
                {
                    Intent intent = new Intent(NETWORK_ACTION);
                    intent.putExtra("action", msg.what);
                    intent.putExtra("data", msg.getData());
                    
                    sendBroadcast(intent);
                    break;
                }
            }
            
            return false;
        }
    }
}
