# Start from a core stack version
FROM jupyter/minimal-notebook
USER root

ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update && apt-get install -y libsm6 libxext6 libturbojpeg  libxrender-dev libjpeg-dev libjpeg8-dev


USER $NB_UID

COPY requirements.txt /tmp/

RUN pip install --requirement /tmp/requirements.txt && \
    fix-permissions $CONDA_DIR && \
    fix-permissions /home/$NB_USER

RUN git clone https://github.com/kylemcdonald/python-utils utils

USER $NB_UID
# CMD ["start-notebook.sh --NotebookApp.token="]
RUN echo "c.NotebookApp.token = u''" >> ~/.jupyter/jupyter_notebook_config.py
