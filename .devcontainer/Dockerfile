ARG fedora_release=41
FROM docker.io/library/fedora:${fedora_release} AS builder
ARG rdkit_git_url=https://github.com/rdkit/rdkit.git
ARG rdkit_git_ref=master

RUN dnf update -y \
  && dnf install -y \
    boost-devel \
    cairo-devel \
    catch-devel \
    cmake \
    eigen3-devel \
    g++ \
    git \
    make \
    zlib-devel \
  && dnf clean all

WORKDIR /opt/RDKit-build

RUN git clone ${rdkit_git_url}

WORKDIR /opt/RDKit-build/rdkit

RUN git checkout ${rdkit_git_ref}

RUN cmake \
    -D CATCH_DIR=/usr/include/catch2 \
    -D RDK_BUILD_CAIRO_SUPPORT=ON \
    -D RDK_BUILD_INCHI_SUPPORT=ON \
    -D RDK_BUILD_AVALON_SUPPORT=ON \
    -D RDK_BUILD_DESCRIPTORS3D=ON \
    -D RDK_BUILD_FREESASA_SUPPORT=ON \
    -D RDK_BUILD_COORDGEN_SUPPORT=ON \
    -D RDK_BUILD_MOLINTERCHANGE_SUPPORT=ON \
    -D RDK_BUILD_YAEHMOP_SUPPORT=ON \
    -D RDK_BUILD_STRUCTCHECKER_SUPPORT=OFF \
    -D RDK_USE_URF=ON \
    -D RDK_BUILD_PGSQL=OFF \
    -D RDK_BUILD_PYTHON_WRAPPERS=OFF \
    -D RDK_INSTALL_INTREE=OFF \
    -D RDK_INSTALL_STATIC_LIBS=ON \
    -D RDK_INSTALL_DEV_COMPONENT=ON \
    -D LIB_SUFFIX=64 \
    -D CMAKE_INSTALL_PREFIX=/usr \
    . 
  
RUN make -j4
RUN \
  RDBASE="$PWD" \
  LD_LIBRARY_PATH="$PWD/lib" \
  ctest -j4 --output-on-failure
RUN make install DESTDIR=/opt/RDKit-build/stage

ARG fedora_release=41
FROM docker.io/library/fedora:${fedora_release}

RUN dnf update -y \
  && dnf install -y \
    boost-iostreams \
    boost-regex \
    boost-serialization \
    boost-system \
    cairo \
    zlib \
  && dnf clean all

COPY --from=builder /opt/RDKit-build/stage/usr /usr

# This Dockerfile image has a non-root user with sudo access. Use the "remoteUser"
# property in devcontainer.json to use it. On Linux, the container user's GID/UIDs
# will be updated to match your local UID/GID (when using the dockerFile property).
# See https://aka.ms/vscode-remote/containers/non-root-user for details.
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

#ENV IMAGE_USER vscode

# Configure and install packages
RUN dnf install -y \
    boost-devel \
    #cairo-devel \
    catch-devel \
    cmake \
    cppcheck \
    findutils \
    eigen3-devel \
    g++ \
    gdb \
    git \
    litecli \
    make \
    python3-ipython \
    python3-rstcheck \
    python3-sphinx \
    # python3-apsw \
    sqlite-devel \
    valgrind \
    which \
    zlib-devel \
    #
    # Create a non-root user to use if preferred - see https://aka.ms/vscode-remote/containers/non-root-user.
    && groupadd --gid $USER_GID $USERNAME \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME \
    # [Optional] Add sudo support for the non-root user
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME\
    && chmod 0440 /etc/sudoers.d/$USERNAME
