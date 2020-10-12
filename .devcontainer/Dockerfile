ARG fedora_release=32
ARG rdkit_release=2020.03.6
#FROM rvianello/fedora-rdkit-cpp:${fedora_release}-${rdkit_release}
FROM fedora-rdkit-cpp-dev:latest

# This Dockerfile image has a non-root user with sudo access. Use the "remoteUser"
# property in devcontainer.json to use it. On Linux, the container user's GID/UIDs
# will be updated to match your local UID/GID (when using the dockerFile property).
# See https://aka.ms/vscode-remote/containers/non-root-user for details.
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

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
    git \
    make \
    python3-ipython \
    # python3-apsw \
    sqlite-devel \
    valgrind \
    which \
    #
    # Create a non-root user to use if preferred - see https://aka.ms/vscode-remote/containers/non-root-user.
    && groupadd --gid $USER_GID $USERNAME \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME \
    # [Optional] Add sudo support for the non-root user
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME\
    && chmod 0440 /etc/sudoers.d/$USERNAME
