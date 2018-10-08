package main

import (
	"github.com/qiniu/api.v7/auth/qbox"
	"github.com/qiniu/api.v7/storage"
	"net/http"
)

var (
	accessKey = "JAwTPb8dmrbiwt89Eaxa4VsL4_xSIYJoJh4rQfOQ"
	secretKey = "G5mtjT3QzG4Lf7jpCAN5PZHrGeoSH9jRdC96ecYS"
)
var (
	mac qbox.Mac = qbox.NewMac(accessKey, secretKey)
)

func getUptoken(w http.ResponseWriter, req *http.Request) {
	w.Write([]byte("Hello"))

}

func main() {

	http.HandleFunc("/uptoken", getUptoken)
	http.ListenAndServe(":8001", nil)

}
