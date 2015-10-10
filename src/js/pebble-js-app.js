'use strict';

var oauthToken = localStorage.getItem('oauthToken') || '';
var oauthTokenSecret = localStorage.getItem('oauthTokenSecret') || '';

function postTweet(tweet) {
  var data = {
    oauth_token: oauthToken,
    oauth_secret: oauthTokenSecret,
    status: tweet
  };
  var xhr = new XMLHttpRequest();
  xhr.open('POST', 'https://ineal.me/pebble/twitter/api/tweet', true);
  xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
  xhr.onload = function() {
    console.log(xhr.responseText);
    Pebble.sendAppMessage({ success: (xhr.status < 300) });
  };
  xhr.send(serialize(data));
}

Pebble.addEventListener('ready', function() {
  var hasTokens = oauthToken && oauthTokenSecret;
  Pebble.sendAppMessage({ hasTokens: hasTokens });
});

Pebble.addEventListener('appmessage', function(e) {
  var tweet = e.payload.text;
  if (tweet) {
    console.log('tweeting: ' + tweet);
    postTweet(tweet);
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('https://ineal.me/pebble/twitter/api/login');
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  console.log('webviewclosed: ' + JSON.stringify(e.response));
  var data = JSON.parse(decodeURIComponent(e.response));
  if (data.keys) {
    if (data.keys.oauth_token) {
      oauthToken = data.keys.oauth_token;
      localStorage.setItem('oauthToken', oauthToken);
    }
    if (data.keys.oauth_token_secret) {
      oauthTokenSecret = data.keys.oauth_token_secret;
      localStorage.setItem('oauthTokenSecret', oauthTokenSecret);
    }
  }
});

function serialize(obj) {
  var s = [];
  for (var p in obj) {
    if (obj.hasOwnProperty(p)) {
      s.push(encodeURIComponent(p) + '=' + encodeURIComponent(obj[p]));
    }
  }
  return s.join('&');
}
