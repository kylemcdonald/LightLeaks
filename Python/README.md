Build image

```
 docker build --rm -t jupyter/lightleaks-notebook .
```

Run image
```
docker run -p 8888:8888 \
-v "$PWD/":/home/jovyan/Lightleaks  \
-v "$PWD/../SharedData/":/home/jovyan/SharedData  \
jupyter/lightleaks-notebook
```

Save/load image for local file transfer

```
docker save -o image.bin jupyter/lightleaks-notebook
docker load -i image.bin
```