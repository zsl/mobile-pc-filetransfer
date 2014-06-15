package com.zsl.file_transfer;

import android.os.Bundle;
import android.app.Activity;
import android.app.ProgressDialog;
import android.view.Menu;

public class ComputerActivity extends Activity {

	PcInfoMgr pcInfoMgr = new PcInfoMgr();
	ProgressDialog progressDlg;
	int count;
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_computer);
		
		pcInfoMgr.run();
		
/*		progressDlg = new ProgressDialog(ComputerActivity.this);
		progressDlg.setTitle("搜索电脑");
		progressDlg.setMessage("正在搜索电脑，请稍后...");
		progressDlg.setCancelable(false);
		progressDlg.show();
		
        new Thread()   
        {  
            public void run()  
            {  
                try  
                {  
                    while (count <= 100)  
                    {   
                        // 由线程来控制进度。  
                        progressDlg.setProgress(count++);  
                        Thread.sleep(100);   
                    }  
                    progressDlg.cancel();  
                }  
                catch (InterruptedException e)  
                {  
                	progressDlg.cancel();  
                }  
            }  
    }.start();    
*/
		
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.computer, menu);
		return true;
	}

}
