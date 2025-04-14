```shell
apt-get update && apt-get install -y zookeeper

# scheduler
/usr/share/zookeeper/bin/zkServer.sh stop  \
&& /usr/share/zookeeper/bin/zkServer.sh start  \
&& /usr/share/zookeeper/bin/zkCli.sh create /scheduler 'scheduler'  \
&& /usr/share/zookeeper/bin/zkCli.sh get /scheduler \
&& python mytest.py --task_name=scheduler --zk_addr=zfs://localhost:2181/scheduler --ps_num=3 --ps_cpu_cores=30 --ps_memory_m=64000 --ckpt_dir=.

# ps
python mytest.py --task_name=ps --zk_addr=zfs://0.0.0.0:2181/scheduler --task_index=0

# worker
python mytest.py --task_name=worker --zk_addr=zfs://0.0.0.0:2181/scheduler --task_index=0 --task_num=2
```