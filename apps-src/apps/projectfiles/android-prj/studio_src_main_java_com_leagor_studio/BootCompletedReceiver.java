package com.leagor.studio;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class BootCompletedReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent.getAction().equals("android.intent.action.BOOT_COMPLETED")) {
            class BootThread implements Runnable {
                private Context context;
                BootThread(Context context) {
                    this.context = context;
                }
                @Override
                public void run() {
                    try {
                        Thread.sleep(5000);
                    } catch (InterruptedException e) { }

                    Intent intent2 = new Intent(context, app.class);
                    intent2.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    context.startActivity(intent2);
                }
            }

            Thread thread = new Thread(new BootThread(context), "BootThread");
            thread.start();
        }
    }
}