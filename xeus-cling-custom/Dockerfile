#
# create a cling-jupyter notebook service
# 
# for more information, see these sources:
# https://blog.jupyter.org/interactive-workflows-for-c-with-jupyter-fe9b54227d92
# https://docs.anaconda.com/anaconda/user-guide/tasks/integration/docker
# https://github.com/QuantStack/xeus-cling
#
# This file partially replicates the continuumio/miniconda3 Dockerfile
# in order to have the latest version if debian included without waiting
# for the miniconda3 docker image to update.
# 

# start of continuumio/miniconda3

FROM ubuntu:18.04
SHELL ["/bin/bash", "-c"]
COPY sources.list /etc/apt/
COPY apt.conf /etc/apt/
COPY .mambarc /etc/mamba/mambarc
COPY micromamba-1.4.9-0 /usr/bin/micromamba

RUN  useradd -ms /bin/bash notebooker && mkdir -p /home/notebooker

RUN rm -rf /var/lib/apt/lists/* /var/log/dpkg.log \
    && apt-get -qq update && apt-get -qq upgrade \
    && apt install -y -qq ca-certificates \
    && update-ca-certificates --fresh \
    && micromamba config sources \
    && micromamba config list \
    && micromamba clean --all --yes

USER notebooker

RUN mkdir -p /home/notebooker/micromamba \
    && export MAMBA_ROOT_PREFIX=/home/notebooker/micromamba \
    && eval "$(micromamba shell hook --shell bash)" \
    && micromamba activate \
    && micromamba env list \
    && micromamba install xeus-cling==0.15.3 -y -c conda-forge \
    && micromamba install notebook==7.0.2 -y -c conda-forge \
    && micromamba install libcxx==9.0.1 -y -c conda-forge \
    && micromamba install libcxxabi==9.0.1 -y -c conda-forge \
    && micromamba clean --all --yes

USER root

RUN apt-get -qq -y remove curl \
    && apt-get -qq -y autoremove \
    && apt-get autoclean \
    && rm -rf /var/lib/apt/lists/* /var/log/dpkg.log

COPY    start-notebook.sh /usr/bin
USER    notebooker
COPY    test.cpp /home/notebooker
WORKDIR /home/notebooker
CMD     mkdir -p /work \
	&& cd /work \
	&& export MAMBA_ROOT_PREFIX=/home/notebooker/micromamba \
	&& eval "$(micromamba shell hook --shell bash)" \
	&& micromamba activate \
	&& jupyter lab --ip='*' --port=8888 --no-browser
