:normal ggzR
:while search('namespace \w\+\(::\w\+\)\+ {') > 0
:  normal f:2xi { namespace %a}%^
:endwhile
:wq
