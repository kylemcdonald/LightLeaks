# This docker file can run the python cli commands, the notebooks 
# and the webserver for camamokjs

# Start from a core stack version
FROM jupyter/minimal-notebook:612aa5710bf9
USER root

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y libsm6 libxext6 libturbojpeg  libxrender-dev libjpeg-dev libjpeg8-dev libgl1-mesa-glx

USER $NB_UID

COPY python/requirements.txt /tmp/

RUN pip install --requirement /tmp/requirements.txt && \
    fix-permissions $CONDA_DIR && \
    fix-permissions /home/$NB_USER

RUN git clone https://github.com/kylemcdonald/python-utils utils
RUN cd utils; git checkout e1e84ba; cd ..

ENV PYTHONPATH="${PYTHONPATH}:/home/jovyan/"

RUN mkdir /home/jovyan/camamokjs 
COPY camamokjs/package.json /home/jovyan/camamokjs/package.json

RUN cd camamokjs; npm install

USER $NB_UID
# CMD ["start-notebook.sh --NotebookApp.token="]
RUN echo "c.NotebookApp.token = u''" >> ~/.jupyter/jupyter_notebook_config.py
