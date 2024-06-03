# nfstop

This utility is designed to track file-level events on both NFS clients and servers, providing insights into file activity within a networked file system environment. Written in C, it leverages Linux syscalls, particularly `fanotify`, to monitor file-level operations. Additionally, it utilizes efficient SQLite for logging, facilitating easy retrieval and analysis of past data. The interface is presented using ncurses in the terminal, offering a user-friendly display of relevant statistics and events.

# Requirements
Linux kernel version 5.1 or above, as it incorporates `FAN_REPORT_FID`, enabling receipt of events with additional information about the underlying filesystem objects.

# Features
 - Real-time monitoring of file-level events on NFS clients and servers.
 - Display of CPU and memory usage of NFS processes.
 - Count of file-level events, providing insights into file access and modifications.
 - Planned: Network statistics, system load, and I/O statistics of NFS client and server processes.
