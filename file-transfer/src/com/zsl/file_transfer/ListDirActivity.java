package com.zsl.file_transfer;

import java.io.File;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;

import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

public class ListDirActivity extends Activity {

    private String computerName;
    private String dirName;
    
    private NetworkService networkService;
    private ServiceConnection serviceConn = new ServiceConnection() {
        
        @Override
        public void onServiceDisconnected(ComponentName name) {
            // TODO Auto-generated method stub
            
        }
        
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            // TODO Auto-generated method stub
            // 连接成功后，开始列出目录
            networkService = ((NetworkService.NetworkBinder)service).getService();
            //
            if (dirName.equals("")){
                if (!networkService.isConnectComputer(computerName)){
                    networkService.connect(computerName, 10000);
                }
                else{
                    networkService.ls(computerName, dirName);
                }
            }
            else{
                networkService.ls(computerName, dirName);
            }
        }
    };
    
    private BroadcastReceiver serviceReceiver = new BroadcastReceiver() {
        
        @Override
        public void onReceive(Context context, Intent intent) {
            // TODO Auto-generated method stub
            int action = intent.getExtras().getInt("action");
            Bundle bundle = intent.getExtras().getBundle("data");
            
            switch (action){
                case PcinfoCmdFactory.CMD_CONNECTED:
                {
                    boolean status = bundle.getBoolean("status");
                    
                    if (status){
                        // 如果为true，那么不应该发送广播消息
                        assert false;
                    }
                    else{
                        String errmsg = bundle.getString("errmsg");
                        System.out.println(errmsg);
                    }
                    
                    break;
                }
                case PcinfoCmdFactory.CMD_DIRS:
                {
                    boolean status = bundle.getBoolean("status");
                    
                    if (status){
                        String parentDir = bundle.getString("parent");
                        
                        if (!parentDir.isEmpty() && !parentDir.endsWith("\\") && !parentDir.endsWith("/")){
                            parentDir = parentDir + "/";
                        }
                        
                        dirName = parentDir;
                        
                        String[] dirs = bundle.getStringArray("dirs");
                        String[] types = bundle.getStringArray("types");
                        
                        List<DirItem> data = dirListAdapter.GetData();
                        data.clear();
                        
                        for (int i = 0; i < dirs.length; ++i){
                            DirItem item = new DirItem();
                            item.type = types[i];
                            item.name = dirs[i];
                            item.fullPath = parentDir + item.name;
                            
                            data.add(item);
                        }
                        
                        // 通知更新了
                        dirListAdapter.notifyDataSetChanged();
                    }
                    else{
                        System.out.println(String.format("errmsg: %s", bundle.getString("errmsg")));
                    }
                    
                    break;
                }
            }
        }
    };
    
    private ListView listView;
    private DirListAdapter dirListAdapter;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_list_dir);
        
        dirListAdapter = new DirListAdapter(this);
        // 初始化界面
        Intent intent = getIntent();
        Bundle bundle = intent.getExtras();
        
        computerName = bundle.getString("computer_name");
        dirName = bundle.getString("dir_name");
        
        this.setTitle(computerName);
        
        // 获取ListView
        listView = (ListView)findViewById(R.id.listview_dir);
        assert listView != null;
        
        listView.setAdapter(dirListAdapter);
        listView.setOnItemClickListener(new OnItemClickListener(){

            @Override
            public void onItemClick(AdapterView<?> parent, View view, int pos,
                    long id) {
                // TODO Auto-generated method stub
                DirItem item = (DirItem)view.getTag();
                
                if (item.type.equals("dir")){
                    networkService.ls(computerName, item.fullPath);
                }
            }
            
        });
        
        // 先收听广播
        IntentFilter filter = new IntentFilter(com.zsl.file_transfer.NetworkService.NETWORK_ACTION);
        registerReceiver(serviceReceiver, filter);
        
        // 绑定服务
        Intent service = new Intent(ListDirActivity.this, NetworkService.class); 
        bindService(service, serviceConn, Context.BIND_AUTO_CREATE);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.list_dir, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item){
        
        if (item.getItemId() == R.id.action_back){
            if (!dirName.isEmpty()){
                File filePath = new File(dirName.replace('\\', '/'));
                String parentDir = filePath.getParent();
                
                if (parentDir == null) parentDir = "";
                
                if (!parentDir.isEmpty() && !parentDir.endsWith("/")){
                    parentDir += '/';
                }
                
                networkService.ls(computerName, parentDir);
            }
        }
        
        return false;
    }
    
    private class DirItem{
        public String type;      // dir or file
        public String name;      // 显示的名字
        public String fullPath;  // 全路径
    }
    
    private class DirListAdapter extends BaseAdapter{

        LayoutInflater mInflater;
        
        private List<DirItem> dirFiles = new ArrayList<DirItem>();
        
        public List<DirItem> GetData(){ return dirFiles; }
        
        public DirListAdapter(Context context){
            super();
            
            mInflater = LayoutInflater.from(context);
        }
        
        @Override
        public int getCount() {
            // TODO Auto-generated method stub
            return dirFiles.size();
        }

        @Override
        public Object getItem(int position) {
            // TODO Auto-generated method stub
            return dirFiles.get(position);
        }

        @Override
        public long getItemId(int position) {
            // TODO Auto-generated method stub
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            // TODO Auto-generated method stub
            
            View view = convertView;
            if (view == null){
                
                //view = new TextView(ListDirActivity.this);
                view = mInflater.inflate(R.layout.list_dir_item, parent, false);
            }
            
            DirItem item = dirFiles.get(position);
            
            ImageView imgFileIcon = (ImageView)view.findViewById(R.id.img_file_icon);
            
            imgFileIcon.setImageResource(item.type.equals("dir") ? R.drawable.folder : R.drawable.file_icon_default);
            
            TextView textFileName = (TextView)view.findViewById(R.id.text_file_name);
            textFileName.setText(item.name);
            //view.setTextSize(25);
            //view.setPadding(10, 0, 0, 0);
            
            view.setTag(item);
            
            return view;
        }
        
    }
}
