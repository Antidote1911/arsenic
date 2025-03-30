FROM archlinux:latest

# Update the package database and install necessary packages
RUN pacman -Syu --noconfirm \
    && pacman -S --noconfirm base-devel

# Add any additional setup or packages you need here
# Example: RUN pacman -S --noconfirm git

RUN pacman -S --noconfirm git cmake clang ninja qt6-base qt6-tools python
RUN git clone https://github.com/Antidote1911/arsenic_cmake
RUN /bin/sh -c 'cd /arsenic_cmake && ./build.sh'
