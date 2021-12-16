# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.205.2/containers/ubuntu/.devcontainer/base.Dockerfile

# [Choice] Ubuntu version (use hirsuite or bionic on local arm64/Apple Silicon): hirsute, focal, bionic
ARG VARIANT="hirsute"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

WORKDIR /dependencies
RUN wget --progress=dot:giga https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2

WORKDIR /opt
RUN tar xf /dependencies/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2 \
    && rm -rf gcc-arm-none-eabi-10-2020-q4-major-x86_64/share/doc

ENV PATH="/opt/gcc-arm-none-eabi-10-2020-q4-major/bin:${PATH}"

# [Optional] Uncomment this section to install additional OS packages.
# RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
#     && apt-get -y install --no-install-recommends <your-package-list-here>
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    libncurses5 \
    cppcheck \
    clang-tidy \
    clang-format \
    && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*


