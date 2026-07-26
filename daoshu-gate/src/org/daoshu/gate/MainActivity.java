package org.daoshu.gate;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        startService(new Intent(this, GateService.class));
        String url = getSharedPreferences("gate", MODE_PRIVATE)
                .getString("world_url", "http://localhost:8899/index.html");
        Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        i.setPackage("com.oculus.browser");
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startActivity(i);
        } catch (Exception e) {
            i.setPackage(null);
            try { startActivity(i); } catch (Exception ignored) {}
        }
        finish();
    }
}
