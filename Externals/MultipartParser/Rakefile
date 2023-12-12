task :default => 'multipart'

file 'multipart' => ['multipart.cpp', 'MultipartParser.h', 'MultipartReader.h'] do
	sh 'g++ -Wall -g multipart.cpp -o multipart'
end

file 'random' do
	sh "dd if=/dev/urandom of=random bs=1048576 count=100"
end

desc "Create a test multipart file"
task :generate_test_file => 'random' do
	output   = ENV['OUTPUT']
	size     = (ENV['SIZE'] || 1024 * 1024 * 100).to_i
	boundary = ENV['BOUNDARY'] || '-----------------------------168072824752491622650073'
	raise 'OUTPUT must be specified' if !output
	
	puts "Creating #{output}"
	File.open(output, 'wb') do |f|
		f.write("--#{boundary}\r\n")
		f.write("content-type: text/plain\r\n")
		f.write("content-disposition: form-data; name=\"field1\"; filename=\"field1\"\r\n")
		f.write("foo-bar: abc\r\n")
		f.write("x: y\r\n")
		f.write("\r\n")
	end
	sh "cat random >> #{output}"
	puts "Postprocessing #{output}"
	File.open(output, 'ab') do |f|	
		f.write("\r\n--#{boundary}--\r\n")
	end
end

task :benchmark => 'multipart' do
	sh "./multipart"
	sh "ruby rack-parser.rb"
	sh "node formidable_parser.js"
end