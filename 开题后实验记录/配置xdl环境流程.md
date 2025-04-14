# 流程

## 1. 安装docker

```shell
sudo apt-get install -y containerd
sudo apt-get install -y docker.io

sudo apt-get install -y nvidia-container-toolkit
```

## 2. docker 换镜像源 

https://cloud.tencent.com/developer/article/2485043
https://docker.xuanyuan.me/

```shell
sudo vim /etc/docker/daemon.json <<EOF
{
    "registry-mirrors": [
        "https://docker.1ms.run",
        "https://docker.xuanyuan.me"
    ]
}
EOF

systemctl daemon-reload
systemctl restart docker
```

## 如果重新配置环境可以直接试试这个镜像

```shell
docker pull listar1111/xdl:cuda9_v1.12_base
```

## 3. 利用配置好的 docker 镜像

```shell
sudo docker pull listar1111/xdl:cuda10_410.79_tf_1.12
```

配置cuda9.0，参考xdl官方手册中的配置方式安装cuda和cudnn
注意：.pub公钥服务器上无法直接下载，需要先下载到本地，再上传到服务器上

## 4. 运行镜像之后

```shell
cd /root
git clone --recursive https://github.com/alibaba/x-deeplearning.git
cd x-deeplearning/xdl
mkdir build && cd build
export CC=/usr/bin/gcc-5 && export CXX=/usr/bin/g++-5
```

## 5. 编译

```shell
cmake .. -DUSE_GPU=1 -DTF_BACKEND=1 -DCUDA_PATH=/usr/local/cuda-9.0 -DNVCC_C_COMPILER=/usr/bin/gcc-4.8 
make -j$(nproc)
# 安装
make install_python_lib
```


















