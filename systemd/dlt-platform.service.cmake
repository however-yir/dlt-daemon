[Unit]
Description=DLT Platform Daemon (Cockpit Logging)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=@DLT_USER@
Group=@DLT_USER@
ExecStart=@CMAKE_INSTALL_PREFIX@/@CMAKE_INSTALL_BINDIR@/dlt-daemon -c @CMAKE_INSTALL_PREFIX@/@CMAKE_INSTALL_SYSCONFDIR@/dlt.conf
ExecStartPost=@CMAKE_INSTALL_PREFIX@/@CMAKE_INSTALL_BINDIR@/dlt-healthcheck.sh /tmp/dlt-ctrl.sock 3490
Restart=on-failure
RestartSec=2
EnvironmentFile=-@CMAKE_INSTALL_PREFIX@/@CMAKE_INSTALL_SYSCONFDIR@/dlt/platform.env

[Install]
WantedBy=multi-user.target
