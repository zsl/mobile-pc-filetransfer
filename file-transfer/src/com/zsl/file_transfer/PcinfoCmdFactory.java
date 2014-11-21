package com.zsl.file_transfer;

import java.util.concurrent.Callable;

import android.os.Handler;

public class PcinfoCmdFactory {
    public static final int CMD_CONNECTED = 0;
    public static final int CMD_DIRS      = 1;
    
    private Handler handler ;
    
    public PcinfoCmdFactory(Handler handler){
        this.handler = handler;
    }
    
    public Callable<PcConnect> newConnectCmd(final String pcname, final int port){
        return new Callable<PcConnect>(){
            
            @Override
            public PcConnect call(){
                
                PcConnect connect = new PcConnect(handler);
                connect.connect(pcname, port);
                
                return connect;
            }
            
        };
    }
    
    public Runnable newLsCmd(final PcConnect connect, final String dir){
        return new Runnable(){

            @Override
            public void run() {
                connect.ls(dir);
            }
            
        };
    }
    
    public Runnable newDownloadCmd(final PcConnect connect, final String fileFullPath, final String savePath){
        return new Runnable(){

            @Override
            public void run() {
                connect.download(fileFullPath, savePath);
            }
            
        };
    }
}
