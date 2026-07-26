package org.daoshu.gate;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Localhost HTTP bridge (port 8901): app enumeration + launch for the daoshu world page. */
public class GateService extends Service {
    private static final String TAG = "DaoshuGate";
    private static final int PORT = 8901;
    private ServerSocket server;
    private volatile boolean running = false;

    @Override
    public void onCreate() {
        super.onCreate();
        startForegroundNotification();
        running = true;
        new Thread(new Runnable() { public void run() { serve(); } }, "gate-http").start();
    }

    private void startForegroundNotification() {
        String ch = "gate";
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            nm.createNotificationChannel(new NotificationChannel(ch, "道枢主页", NotificationManager.IMPORTANCE_LOW));
        }
        Notification n = new Notification.Builder(this, "gate")
                .setContentTitle("道枢主页层运行中")
                .setContentText("localhost:8901 应用之门")
                .setSmallIcon(android.R.drawable.ic_menu_compass)
                .build();
        startForeground(1, n);
    }

    private void serve() {
        try {
            server = new ServerSocket(PORT, 8, java.net.InetAddress.getByName("127.0.0.1"));
            while (running) {
                final Socket s = server.accept();
                new Thread(new Runnable() { public void run() { handle(s); } }).start();
            }
        } catch (Exception e) {
            Log.e(TAG, "server", e);
        }
    }

    private void handle(Socket s) {
        try (Socket sock = s) {
            BufferedReader in = new BufferedReader(new InputStreamReader(sock.getInputStream(), StandardCharsets.UTF_8));
            String line = in.readLine();
            if (line == null) return;
            String[] parts = line.split(" ");
            String path = parts.length > 1 ? parts[1] : "/";
            while ((line = in.readLine()) != null && !line.isEmpty()) { /* drain headers */ }

            String body;
            if (path.startsWith("/apps")) {
                body = appsJson();
            } else if (path.startsWith("/launch")) {
                body = launch(query(path).get("pkg"));
            } else if (path.startsWith("/view")) {
                body = view(query(path).get("url"));
            } else {
                body = "{\"ok\":true,\"service\":\"daoshu-gate\",\"port\":" + PORT + "}";
            }
            byte[] b = body.getBytes(StandardCharsets.UTF_8);
            OutputStream out = sock.getOutputStream();
            out.write(("HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=utf-8\r\n"
                    + "Access-Control-Allow-Origin: *\r\nContent-Length: " + b.length + "\r\nConnection: close\r\n\r\n")
                    .getBytes(StandardCharsets.UTF_8));
            out.write(b);
            out.flush();
        } catch (Exception e) {
            Log.e(TAG, "handle", e);
        }
    }

    private Map<String, String> query(String path) {
        Map<String, String> m = new HashMap<>();
        int q = path.indexOf('?');
        if (q < 0) return m;
        for (String kv : path.substring(q + 1).split("&")) {
            int eq = kv.indexOf('=');
            if (eq > 0) {
                try {
                    m.put(kv.substring(0, eq), URLDecoder.decode(kv.substring(eq + 1), "UTF-8"));
                } catch (Exception ignored) {}
            }
        }
        return m;
    }

    private String appsJson() {
        PackageManager pm = getPackageManager();
        Map<String, String> apps = new HashMap<>();
        String[][] filters = {
                {Intent.ACTION_MAIN, Intent.CATEGORY_LAUNCHER},
                {Intent.ACTION_MAIN, "com.oculus.intent.category.VR"},
        };
        for (String[] f : filters) {
            Intent it = new Intent(f[0]);
            it.addCategory(f[1]);
            for (ResolveInfo ri : pm.queryIntentActivities(it, 0)) {
                ApplicationInfo ai = ri.activityInfo.applicationInfo;
                CharSequence label = pm.getApplicationLabel(ai);
                apps.put(ai.packageName, label == null ? ai.packageName : label.toString());
            }
        }
        StringBuilder sb = new StringBuilder("[");
        boolean first = true;
        for (Map.Entry<String, String> e : apps.entrySet()) {
            if (!first) sb.append(',');
            first = false;
            sb.append("{\"pkg\":\"").append(esc(e.getKey())).append("\",\"label\":\"").append(esc(e.getValue())).append("\"}");
        }
        return sb.append(']').toString();
    }

    private String launch(String pkg) {
        if (pkg == null) return "{\"ok\":false,\"err\":\"pkg required\"}";
        try {
            PackageManager pm = getPackageManager();
            Intent i = pm.getLaunchIntentForPackage(pkg);
            if (i == null) {
                i = new Intent(Intent.ACTION_MAIN);
                i.addCategory("com.oculus.intent.category.VR");
                i.setPackage(pkg);
                List<ResolveInfo> ris = pm.queryIntentActivities(i, 0);
                if (ris.isEmpty()) return "{\"ok\":false,\"err\":\"no launch intent\"}";
                i.setClassName(pkg, ris.get(0).activityInfo.name);
            }
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(i);
            return "{\"ok\":true,\"pkg\":\"" + esc(pkg) + "\"}";
        } catch (Exception e) {
            return "{\"ok\":false,\"err\":\"" + esc(String.valueOf(e)) + "\"}";
        }
    }

    private String view(String url) {
        if (url == null) return "{\"ok\":false,\"err\":\"url required\"}";
        try {
            Intent i = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(i);
            return "{\"ok\":true}";
        } catch (Exception e) {
            return "{\"ok\":false,\"err\":\"" + esc(String.valueOf(e)) + "\"}";
        }
    }

    private static String esc(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) { return START_STICKY; }

    @Override
    public void onDestroy() {
        running = false;
        try { if (server != null) server.close(); } catch (Exception ignored) {}
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
