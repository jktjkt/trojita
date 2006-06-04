from ymaplib import TCPStream, CRLF
import time

s = TCPStream('localhost', 143, 100)
time.sleep(0.2)
s.has_data()
time.sleep(0.2)
s.readline()
time.sleep(0.2)
s.has_data()
print 'startls...'
s.write('0 starttls'+CRLF)
time.sleep(0.2)
print s.readline()
s.starttls()
print s._ssl_connection.state_string()
print 'reading...'
print s.read(1)
print 'blesmrt...'
s.write('1 capability'+CRLF)
print s._ssl_connection.pending()
print s._ssl_connection.state_string()
print "|" + s.read(1) + "|"
print s._ssl_connection.pending()
print s.readline()
s.write('2 capability'+CRLF)
print 'hmm'
print s._ssl_connection.pending()
time.sleep(0.5)
print s.has_data()
print 'blesmrt'
print s._ssl_connection.state_string()
s.close()
s._ssl_connection.shutdown()
print s._ssl_connection.state_string()