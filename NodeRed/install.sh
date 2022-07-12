#!/usr/bin/env bash

# TODO: Convert this to JS, because writing bash is bad for the soul

if [[ ! -f ".env" ]]
then
    echo ".env file not found, cannot install Node-Red modules!"
    exit 1
fi

NODE_RED_DIR=$(grep userDir ./.env | cut -c 9-)

if [[ ! $NODE_RED_DIR ]]
then
    echo "Error: Couldn't read userDir from local .env file!"
    exit 1
fi

npm run tsc 

if [[ $? -ne 0 ]]
then
    echo "Error: tsc returned an error, aborting!"
    exit 1
fi

# TODO: CHECK FOR LEADING SLASHES!
NODE_RED_NODES_DIR="$NODE_RED_DIR/node_modules/automato_nodes"

# Create the directory if it does not exist
# This should only happen on first install
# before node-red has ever ran.
mkdir -p $NODE_RED_NODES_DIR

rm -rf $NODE_RED_NODES_DIR/*
cp -R ./package.json $NODE_RED_NODES_DIR

JS_NODE_FILES=$(ls ./dist/nodes/*/*.js | grep -vE "/common_browser|common_server|/@types/")
COMMON_JS_BROWSER_FILES=$(ls ./dist/nodes/common_browser/*.js)
COMMON_JS_SERVER_FILES=$(ls ./dist/nodes/common_server/*.js)
HTML_NODE_FILES=$(ls ./src/nodes/*/*.html)

cp -R $JS_NODE_FILES $HTML_NODE_FILES $NODE_RED_NODES_DIR

mkdir $NODE_RED_NODES_DIR/resources

mkdir $NODE_RED_NODES_DIR/common_server

cp -R $COMMON_JS_SERVER_FILES $NODE_RED_NODES_DIR/common_server

cp -R $COMMON_JS_BROWSER_FILES $NODE_RED_NODES_DIR/resources

# For every file in /common_browser (which ends up at /resources)
for com in $(ls $NODE_RED_NODES_DIR/resources)
do
    # remove all lines that contain Object.def...,
    sed -i '/^Object.defineProperty(exports, \"__esModule/d' $NODE_RED_NODES_DIR/resources/$com
    # Remove all lines that start with exports.
    sed -i '/^exports\./d' $NODE_RED_NODES_DIR/resources/$com
done

# For every file in /common_server (which ends up at /common_server)
# for com in $(ls $NODE_RED_NODES_DIR/common_server)
# do
    # remove all lines that contain Object.def...,
    # sed -i '/^Object.defineProperty(exports, \"__esModule/d' $NODE_RED_NODES_DIR/common_server/$com
    # Remove all lines that start with exports.
    # sed -i '/^exports\./d' $NODE_RED_NODES_DIR/common_server/$com
# done

# For every resulting server js file
for com in $(ls $NODE_RED_NODES_DIR/*.js)
do
    # Convert: const x = require("../common_server/x")
    # into: const x = require("./common_server/x")
    sed -i "s/..\/common_server\//.\/common_server\//g" $com
done

for f in $(ls $NODE_RED_NODES_DIR/*.html.*)
do  
    # example.html.js -> example.html
    HTML_FILE=${f::-3}

    # When developing, we use: import { x } from "../common_browser/cool_library"
    # When converting this to html, we need to use: <script src="resources/node-red-automato/cool_library.js">
    # the regex gets the filename "cool_library" from any require() lines
    # Example: const example = require("../common_browser/cool_library") -> cool_library
    REQUIRED_FILES=$(cat $f | grep -Po "(?<=require\(\"..\/common_browser\/).*(?=\")" | tr '\n' ' ')
    for req in ${REQUIRED_FILES} 
    do
        echo \<script type=\"text\/javascript\" src=\"resources\/automato-node-red\/$req.js\"\> >> $HTML_FILE
        echo \<\/script\> >> $HTML_FILE
    done
    
    # Delete lines that start with Object.def...
    # Typescript always emits this, and Node-Red does not like that!
    # Also delete any of the previously mentioned require() lines
    # Also delete "cool_library_1." which typescript emits since
    # it thinks we're using CommonJS modules, but we are not.
    echo \<script type=\"text\/javascript\"\> >> $HTML_FILE
    cat $f \
    | sed '/^Object.defineProperty(exports, \"__esModule/d' \
    | sed '/require(\"..\/common/d' \
    >> $HTML_FILE
    echo \<\/script\> >> $HTML_FILE

    # After we write the JS to the HTML file, we need to delete all 
    # instances of "cool_library_1"
    # typescript emits this in the JS file, as it assumes
    # we're using: export.<function> and import <function>, then cool_library_1.<function>
    # but we're not.
    for req in ${REQUIRED_FILES}
    do
        sed -i "s/${req}_1.//" $HTML_FILE
    done

    rm $f 
done
