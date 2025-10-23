# Use an official Ubuntu image as a base
FROM ubuntu:latest

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Update package list and install build dependencies
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory inside the container
WORKDIR /app

# Copy the entire project directory into the container
COPY . .

# Create a build directory and build the project
RUN cmake -S . -B build && \
    cmake --build build

# Define the default command to run the CLI application
CMD ["./build/nova_db"]
