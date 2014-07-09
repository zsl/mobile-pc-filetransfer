package com.zsl.file_transfer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

import android.os.Bundle;
import android.preference.Preference.OnPreferenceChangeListener;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.Menu;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.EditText;

public class MainActivity extends Activity {

    private AutoCompleteTextView autoTextView;
    private SharedPreferences preference;
    private List<String> ipHistory = new ArrayList<String>();
    
    private OnSharedPreferenceChangeListener preferenceListener = new OnSharedPreferenceChangeListener() {
            
            @Override
            public void onSharedPreferenceChanged(SharedPreferences sharedPreferences,
                    String key) {
                // TODO Auto-generated method stub
                if (key.equals("history")){
                    reloadIpHistory();
                }
            }
        };
    
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		initComponent();
		setClickListener();
		
		// 在这里启动服务
		startService(new Intent(this, com.zsl.file_transfer.NetworkService.class));
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
		return true;
	}

	private void setClickListener()
	{
	    
	    // 搜索电脑
		Button btnSearchComputer = (Button)findViewById(R.id.btn_search_computer);
		btnSearchComputer.setOnClickListener(new OnClickListener(){

			@Override
			public void onClick(View v) {
				Intent intent = new Intent();
				intent.setClass(MainActivity.this, ComputerActivity.class);
				startActivity(intent);
				
			}
			
		});
		
		// 连接
		Button btnConnect = (Button)findViewById(R.id.btn_connect_computer);
		btnConnect.setOnClickListener(new OnClickListener(){

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                
                EditText editComputerName = (EditText)findViewById(R.id.edit_ip);
                
                String pcname = editComputerName.getText().toString();
                
                Intent intent = new Intent();
                intent.setClass(MainActivity.this, ListDirActivity.class);
                
                intent.putExtra("computer_name", pcname);
                intent.putExtra("dir_name", "");
                
                startActivity(intent);
                
                if (!pcname.trim().isEmpty()){
                    saveIpHistory(pcname);
                }
            }
		    
		});
		
		// 处理edit的点击事件
		autoTextView.setOnClickListener(new OnClickListener() {
            
            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                AutoCompleteTextView view = (AutoCompleteTextView)v;
                
                view.showDropDown();
            }
        });
	}
	
	private void initComponent(){
	    autoTextView = (AutoCompleteTextView)findViewById(R.id.edit_ip);
	    
	    assert autoTextView != null;
	    
	    autoTextView.setDropDownHeight(LayoutParams.WRAP_CONTENT);
	    
	    preference = getSharedPreferences("ip_history", MODE_PRIVATE);
	    preference.registerOnSharedPreferenceChangeListener(preferenceListener);
	    
	    // 初始化一下它的状态
	    loadIpHistory();
	}
	
    private void loadIpHistory(){
	    if (ipHistory.isEmpty()){
    	    String longHistory = preference.getString("history", "");
    	    
    	    if (!longHistory.isEmpty()){
    	        String[] historyArray = longHistory.split(",");
    	        ipHistory = new ArrayList<String>(Arrays.asList(historyArray));
    	    }
	    }
	    
	    if (!ipHistory.isEmpty()){
	        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, android.R.layout.simple_dropdown_item_1line, ipHistory);
	        autoTextView.setAdapter(adapter);
	    }
	    
	}
	
	private void reloadIpHistory(){
	    
	    String longHistory = preference.getString("history", "");
	    
	    if (!longHistory.isEmpty()){
	        String[] historyArray = longHistory.split(",");
	        ipHistory = Arrays.asList(historyArray);
	    }
	    
	    loadIpHistory();
	}
	
	private void saveIpHistory(String newHistory){
	    int index = ipHistory.indexOf(newHistory);
	    
	    if (-1 != index){
	        
	        if (0 == index){
	            return;
	        }
	        
	        ipHistory.remove(index);
	    }
	    
        ipHistory.add(0, newHistory);
        
        StringBuffer sb = null;
        Iterator<String> it = ipHistory.iterator();
        
        while (it.hasNext()){
            if (sb == null){
                sb = new StringBuffer();
            }
            else{
                sb.append(',');
            }
            
            sb.append(it.next());
        }
        
        preference.edit().putString("history", sb.toString()).commit();
	}
}
