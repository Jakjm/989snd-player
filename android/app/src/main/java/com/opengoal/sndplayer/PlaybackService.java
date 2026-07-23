package com.opengoal.sndplayer;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.IBinder;

public class PlaybackService extends Service {
    static final String ACTION_START = "com.opengoal.sndplayer.START";
    static final String ACTION_PAUSE = "com.opengoal.sndplayer.PAUSE";
    static final String ACTION_RESUME = "com.opengoal.sndplayer.RESUME";
    static final String ACTION_STOP = "com.opengoal.sndplayer.STOP";
    static final String EXTRA_COUNT = "count";
    static final String EXTRA_PLAYING = "playing";
    static final String EXTRA_NAME = "name";

    private static final String CHANNEL_ID = "playback";
    private static final int NOTIFICATION_ID = 1;

    private static final int NATIVE_PAUSE_ALL = 0;
    private static final int NATIVE_RESUME_ALL = 1;
    private static final int NATIVE_STOP_ALL = 2;

    private static PlaybackService sInstance;

    private MediaSession mediaSession;

    static void show(Context ctx, int count, boolean playing, String name) {
        PlaybackService inst = sInstance;
        if (inst != null) {
            inst.updateNotification(count, playing, name);
            return;
        }
        Intent intent = new Intent(ctx, PlaybackService.class)
            .setAction(ACTION_START)
            .putExtra(EXTRA_COUNT, count)
            .putExtra(EXTRA_PLAYING, playing)
            .putExtra(EXTRA_NAME, name);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            ctx.startForegroundService(intent);
        } else {
            ctx.startService(intent);
        }
    }

    static void hide() {
        PlaybackService inst = sInstance;
        if (inst != null) {
            inst.stopSelfAndForeground();
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        sInstance = this;
        createChannel();
        mediaSession = new MediaSession(this, "sndplayer");
        mediaSession.setCallback(new MediaSession.Callback() {
            @Override
            public void onPlay() {
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_RESUME_ALL);
            }

            @Override
            public void onPause() {
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_PAUSE_ALL);
            }

            @Override
            public void onStop() {
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_STOP_ALL);
            }
        });
        mediaSession.setActive(true);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent != null ? intent.getAction() : null;
        if (action == null) {
            return START_NOT_STICKY;
        }
        switch (action) {
            case ACTION_START: {
                int count = intent.getIntExtra(EXTRA_COUNT, 0);
                boolean playing = intent.getBooleanExtra(EXTRA_PLAYING, true);
                String name = intent.getStringExtra(EXTRA_NAME);
                startInForeground(buildNotification(count, playing, name));
                break;
            }
            case ACTION_PAUSE:
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_PAUSE_ALL);
                break;
            case ACTION_RESUME:
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_RESUME_ALL);
                break;
            case ACTION_STOP:
                SndPlayerActivity.nativeOnPlaybackAction(NATIVE_STOP_ALL);
                stopSelfAndForeground();
                break;
            default:
                break;
        }
        return START_NOT_STICKY;
    }

    private void startInForeground(Notification notification) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(NOTIFICATION_ID, notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK);
        } else {
            startForeground(NOTIFICATION_ID, notification);
        }
    }

    private void updateNotification(int count, boolean playing, String name) {
        NotificationManager nm =
            (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null) {
            nm.notify(NOTIFICATION_ID, buildNotification(count, playing, name));
        }
    }

    private void stopSelfAndForeground() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE);
        } else {
            stopForeground(true);
        }
        stopSelf();
    }

    private void createChannel() {
        NotificationManager nm =
            (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (nm != null && nm.getNotificationChannel(CHANNEL_ID) == null) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "Playback", NotificationManager.IMPORTANCE_LOW);
            channel.setShowBadge(false);
            nm.createNotificationChannel(channel);
        }
    }

    private PendingIntent servicePendingIntent(String action) {
        Intent intent = new Intent(this, PlaybackService.class).setAction(action);
        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        return PendingIntent.getService(this, action.hashCode(), intent, flags);
    }

    private Notification buildNotification(int count, boolean playing, String name) {
        Intent openIntent = new Intent(this, SndPlayerActivity.class)
            .setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        int piFlags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            piFlags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent contentIntent = PendingIntent.getActivity(this, 0, openIntent, piFlags);

        Notification.Action toggle = playing
            ? new Notification.Action.Builder(android.R.drawable.ic_media_pause, "Pause",
                servicePendingIntent(ACTION_PAUSE)).build()
            : new Notification.Action.Builder(android.R.drawable.ic_media_play, "Resume",
                servicePendingIntent(ACTION_RESUME)).build();
        Notification.Action stop = new Notification.Action.Builder(
            android.R.drawable.ic_menu_close_clear_cancel, "Stop",
            servicePendingIntent(ACTION_STOP)).build();

        String title = (name != null && !name.isEmpty()) ? "989snd Player: " + name : "989snd Player";
        String text = count == 1 ? "1 sound playing" : count + " sounds playing";

        Notification.Builder builder = new Notification.Builder(this, CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentIntent(contentIntent)
            .setOngoing(true)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .addAction(toggle)
            .addAction(stop)
            .setStyle(new Notification.MediaStyle()
                .setMediaSession(mediaSession.getSessionToken())
                .setShowActionsInCompactView(0, 1));

        PlaybackState state = new PlaybackState.Builder()
            .setActions(PlaybackState.ACTION_PLAY | PlaybackState.ACTION_PAUSE
                | PlaybackState.ACTION_STOP)
            .setState(playing ? PlaybackState.STATE_PLAYING : PlaybackState.STATE_PAUSED, 0, 1.0f)
            .build();
        mediaSession.setPlaybackState(state);

        return builder.build();
    }

    @Override
    public void onDestroy() {
        if (mediaSession != null) {
            mediaSession.release();
            mediaSession = null;
        }
        if (sInstance == this) {
            sInstance = null;
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
