FROM amazonlinux:latest
RUN yum update -y
RUN yum install -y gcc tcl-devel readline-devel
RUN curl -s https://www.sqlite.org/2018/sqlite-autoconf-3220000.tar.gz | tar xvz && \
    cd sqlite-autoconf-3220000 && \
    ./configure && \
    make && \
    make install && \
    cd .. && \
    rm -rf sqlite-autoconf-3220000
