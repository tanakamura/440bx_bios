alarm.local:8080/rom へ PUT リクエストを投げると、そのファイルがromへアップロードされる。

 curl -X PUT --data-binary @file http://alarm.local:8080/rom

のようにする。

リセットは今手動なので、人間に頼む必要がある。

 $ notify-send "please reset!!" -t 5000

を出してくれると、私がリセットを押す。
5秒くらい待って何もしてなければ私は離席してるので諦める。
