FROM ubuntu
WORKDIR /app
RUN apt-get update && apt-get install -y gcc
RUN apt-get install -y libgsl-dev
RUN apt-get install -y python3
RUN apt-get install -y pip
RUN apt-get install -y python3-numpy
RUN apt-get install -y python3-scipy