//compile with   javac -cp .:json-20230618.jar:mysql-connector-j-9.3.0.jar server.java
//run with       java -cp .:json-20230618.jar:mysql-connector-j-9.3.0.jar server
//URL		 http://18.116.46.202:8000/ 


import com.sun.net.httpserver.*;
import org.json.JSONObject;

import java.io.*;
import java.net.InetSocketAddress;
import java.sql.*;
import java.util.regex.*;

public class server	{
        private	static final int PORT =	8000;
        private static final String DB_URL = "jdbc:mysql://localhost/restdb";
        private static final String DB_USER = "root";
        private static final String DB_PASS = "password";

        public static void main(String[] args) throws Exception {
                HttpServer server = HttpServer.create(new InetSocketAddress(PORT), 0);
		server.createContext("/", new RootHandler());
		System.out.println("Server started on port " + PORT);
		server.start();
	}

	static class RootHandler implements HttpHandler {
		public void handle(HttpExchange exchange) throws IOException {
			String method = exchange.getRequestMethod();
			String path = exchange.getRequestURI().getPath();
			Connection conn = null;
	
			try {
				conn = DriverManager.getConnection(DB_URL, DB_USER, DB_PASS);
				if (method.equals("POST") || method.equals("PUT")) {
					handlePostPut(exchange, conn);
				} else if (method.equals("GET")) {
					handleGet(exchange, conn, path);
				} else if (method.equals("DELETE")) {
					handleDelete(exchange, conn, path);
				} else {
					sendResponse(exchange, 405, "{\"error\":\"Method not Allowed\"}");
				}
		
			} catch (SQLException e) {
				sendResponse(exchange, 500, "{\"error\":\"Databse error\"}");
				e.printStackTrace();
			} finally {
				if (conn != null) try { conn.close(); } catch (Exception e) {}
			}
		}
	}

	private static void handlePostPut(HttpExchange exchange, Connection conn) throws IOException, SQLException {
		String body = new BufferedReader(new InputStreamReader(exchange.getRequestBody()))
			.lines().reduce("", (acc, line) -> acc + line);
		
		if (body.isEmpty()) {
			sendResponse(exchange, 400, "{\"error\":\"Payload Required\"}");
			return;
		}

		JSONObject json;
		try {
			json = new JSONObject(body);
		} catch (Exception e) {
			sendResponse(exchange, 400, "{\"error\":\"Invalid JSON\"}");
			return;
		}
		
		PreparedStatement stmt = conn.prepareStatement("Insert into items (data) Values (?)", Statement.RETURN_GENERATED_KEYS);
		stmt.setString(1, json.toString());
		stmt.executeUpdate();
		
		ResultSet rs = stmt.getGeneratedKeys();
		int id = rs.next() ? rs.getInt(1) : -1;
		
		sendResponse(exchange, 201, "{\"id\":" + id + "}");
	}
	
	private static void handleGet(HttpExchange exchange, Connection conn, String path) throws IOException, SQLException {
		String id = path.replaceFirst("/", "");
		if (!id.matches("\\d+")) {
			sendResponse(exchange, 400, "{\"error\":\"Invalid ID\"}");
			return;
		}
		
		PreparedStatement stmt = conn.prepareStatement("Select data from items where id = ?");
		stmt.setInt(1, Integer.parseInt(id));
		ResultSet rs = stmt.executeQuery();
		
		if (rs.next()) {
			sendResponse(exchange, 200, rs.getString("data"));
		} else {
			sendResponse(exchange, 404, "{\"error\":\"Not Found\"}");
		}
	}
	
	private static void handleDelete(HttpExchange exchange, Connection conn, String path) throws IOException, SQLException {
		String id = path.replaceFirst("/", "");
		if (!id.matches("\\d+")) {
			sendResponse(exchange, 400, "{\"error\":\"Invalid id\"}");
			return;
		}
		
		PreparedStatement stmt = conn.prepareStatement("Delete from items where id = ?");
		stmt.setInt(1, Integer.parseInt(id));
		int rows = stmt.executeUpdate();

		if (rows > 0) {
			sendResponse(exchange, 200, "{\"status\":\"Deleted\"}");
		} else {
			sendResponse(exchange, 404, "{\"error\":\"Not Found\"}");
		}		

	}

	private static void sendResponse(HttpExchange exchange, int statusCode, String response) throws IOException {
		byte[] bytes = response.getBytes();
		exchange.getResponseHeaders().add("Content-Type", "application/json");
		exchange.sendResponseHeaders(statusCode, bytes.length);
		OutputStream os = exchange.getResponseBody();
		os.write(bytes);
		os.close();
	}
}

