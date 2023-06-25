#
# A simple docker file for building and unit testing
#
# Build with --build-arg="apt_proxy=$apt_proxy" for using an proxy for apt
#

# Our build requirements

FROM amigadev/crosstools:ppc-amigaos

ENTRYPOINT ["tail", "-f", "/dev/null"]