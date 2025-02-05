# This docker file can run the python cli commands, the notebooks 
# and the webserver for camamokjs

# Start from a core stack version
FROM jupyter/minimal-notebook:notebook-6.4.0
USER root

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y libsm6 libxext6 libturbojpeg  libxrender-dev libjpeg-dev libjpeg8-dev libgl1-mesa-glx gcc g++ build-essential llvm-10 llvm-10-dev

USER $NB_UID

# RUN echo python --version

COPY python/requirements.txt /tmp/

RUN LLVM_CONFIG=/usr/bin/llvm-config-10 pip install llvmlite==0.34.0

RUN pip install --requirement /tmp/requirements.txt && \
    fix-permissions $CONDA_DIR && \
    fix-permissions /home/$NB_USER

RUN git clone https://github.com/kylemcdonald/python-utils utils
RUN cd utils; git checkout e1e84ba; cd ..

ENV PYTHONPATH="${PYTHONPATH}:/home/jovyan/"

RUN mkdir /home/jovyan/camamokjs 

# Install nodejs 16
RUN npm -g install npm@9
COPY camamokjs/package.json /home/jovyan/camamokjs/package.json
COPY camamokjs/package-lock.json /home/jovyan/camamokjs/package-lock.json

RUN node --version; npm --version; cd camamokjs; npm ci

USER $NB_UID
# CMD ["start-notebook.sh --NotebookApp.token="]
RUN echo "c.NotebookApp.token = u''" >> ~/.jupyter/jupyter_notebook_config.py
RUN echo "c.NotebookApp.disable_check_xsrf = True" >> ~/.jupyter/jupyter_notebook_config.py

