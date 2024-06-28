const fs = require('fs');
const readline = require('readline');

// 获取命令行参数（除去前两个元素）
const args = process.argv.slice(2);

// 检查参数数量
if (args.length !== 2) {
  console.error("Usage: " + process.argv[0] + " " + process.argv[1] +
                " ./remu.log.sm ./html/node.html.body");
  process.exit(1);
}

// 解构赋值来获取 input 和 output 文件路径
const [inputFile, outputFile] = args;

// 创建读取流
const readStream = fs.createReadStream(inputFile);

// 创建写入流
const writeStream = fs.createWriteStream(outputFile, {flags : 'w+'});

// 使用 readline 处理逐行读取
const rl = readline.createInterface({
  input : readStream,
  crlfDelay : Infinity // 此设置支持 Windows (CRLF) 和 UNIX (LF) 系统的换行模式
});

var write_table_begin = false;
var node_names = {};

const td_str = "\t\t<td>";
const td_change_str = "\t\t<td class=\"change\">";
const td3_str = "\t\t<td colspan=\"3\">"
const td3_change_str = "\t\t<td class=\"change\" colspan=\"3\">"
const td_end_str = "</td>\n";

rl.on('line', (line) => {
  // console.log(line);  // 在控制台打印每一行
  // writeStream.write(line + '\n');  // 将每一行写入文件，添加换行符

  if (line != "") {
    if (!write_table_begin) {
      writeStream.write("<table>\n");
      writeStream.write("\t<tr>\n");
      writeStream.write("\t\t<td class=\"nodes\">Nodes:</td>\n");
      write_table_begin = true;
    }

    var parts = line.split(/\s+/);
    var event_name = parts[1];

    if (event_name == "state_change:") {
      var node_name = parts[6];
      if (node_name != "") {
        node_names[node_name] = node_name;
      }
    }
  }
});

rl.on('close', () => {
  // console.log('File reading and writing completed.');

  var keys = Object.keys(node_names);
  for (var element of keys) {
    writeStream.write("\t\t<td class=\"nodes\">")
    // console.log("+++" + element + "----")
    writeStream.write(element);
    writeStream.write(td_end_str);
  }

  writeStream.write("\t</tr>\n");
  writeStream.write("</table>\n");

  writeStream.end(); // 关闭写入流
});

// 错误处理
readStream.on(
    'error',
    error => { console.error('Error reading the input file:', error); });
writeStream.on(
    'error',
    error => { console.error('Error writing to the output file:', error); });