#!/bin/bash
set -e
GIT_TAG=$(git rev-parse --short HEAD)
git tag -fa v1.11-${GIT_TAG} -m "v1.11-${GIT_TAG}"

