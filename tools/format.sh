#!/bin/sh

cd $(dirname $0)/..

clang-format -i include/td365/* src/* examples/*
