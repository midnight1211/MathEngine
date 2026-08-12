package com.mathengine.server.auth;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.http.HttpMethod;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.web.SecurityFilterChain;
import org.springframework.security.web.authentication.UsernamePasswordAuthenticationFilter;
import org.springframework.web.cors.CorsConfiguration;
import org.springframework.web.cors.CorsConfigurationSource;
import org.springframework.web.cors.UrlBasedCorsConfigurationSource;

import java.util.List;

/**
 * SecurityConfig
 * ───────────────
 * Configures Spring Security for stateless JWT authentication.
 *
 * Public endpoints (no token required):
 *   POST /api/auth/register
 *   POST /api/auth/login
 *   POST /api/compute          <- guests can compute without signing in
 *   POST /api/chat             <- guests can use the chatbot without signing in
 *   GET  /api/chat/status
 *   GET  /api/docs/search      <- knowledge-base lookups, same as the desktop path
 *   GET  /                     <- mobile web UI
 *   GET  /static/**            <- CSS / JS assets
 *
 * Protected endpoints (valid JWT required):
 *   GET  /api/history          <- sync history
 *   POST /api/history          <- save entry
 *   DELETE /api/history/**     <- delete entry
 *   GET  /api/preferences      <- load preferences
 *   PUT  /api/preferences      <- save preferences
 *   POST /api/export           <- export (guests can export too — change if needed)
 */
@Configuration
@EnableWebSecurity
public class SecurityConfig {

    @Value("${mathengine.cors.allowed-origins}")
    private String allowedOrigins;

    private final JwtFilter jwtFilter;

    public SecurityConfig(JwtFilter jwtFilter) {
        this.jwtFilter = jwtFilter;
    }

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        http
            // Stateless — no sessions, no CSRF needed
            .sessionManagement(s -> s.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .csrf(csrf -> csrf.disable())

            // CORS — allows the JavaFX client and mobile browsers to call the API
            .cors(cors -> cors.configurationSource(corsConfigurationSource()))

            // Route permissions
            .authorizeHttpRequests(auth -> auth
                // Auth endpoints — always public
                .requestMatchers(HttpMethod.POST, "/api/auth/register").permitAll()
                .requestMatchers(HttpMethod.POST, "/api/auth/login").permitAll()

                // Compute — public (guests can use the engine)
                .requestMatchers(HttpMethod.POST, "/api/compute").permitAll()

                // Chat — public (guests can use the chatbot, same as compute).
                // ChatController completes wiring ChatbotService/Models.ChatRequest
                // already had in place; see ChatController's javadoc.
                .requestMatchers(HttpMethod.POST, "/api/chat").permitAll()
                .requestMatchers(HttpMethod.GET, "/api/chat/status").permitAll()

                // Docs knowledge-base search — public, same rationale as
                // chatbot/knowledge.py needing no auth on the desktop path
                .requestMatchers(HttpMethod.GET, "/api/docs/search").permitAll()

                // Export — public
                .requestMatchers(HttpMethod.POST, "/api/export").permitAll()

                // Static web UI files — public
                .requestMatchers("/", "/index.html", "/css/**", "/js/**",
                                 "/favicon.ico").permitAll()

                // Everything else requires a valid JWT
                .anyRequest().authenticated()
            )

            // JWT filter runs before Spring's username/password filter
            .addFilterBefore(jwtFilter, UsernamePasswordAuthenticationFilter.class);

        return http.build();
    }

    @Bean
    public PasswordEncoder passwordEncoder() {
        // BCrypt with strength 12 — strong but not noticeably slow for a capstone
        return new BCryptPasswordEncoder(12);
    }

    @Bean
    public CorsConfigurationSource corsConfigurationSource() {
        CorsConfiguration config = new CorsConfiguration();

        // Allow configured origins (default: * for local network use)
        if ("*".equals(allowedOrigins)) {
            config.addAllowedOriginPattern("*");
        } else {
            config.setAllowedOrigins(List.of(allowedOrigins.split(",")));
        }

        config.setAllowedMethods(List.of("GET", "POST", "PUT", "DELETE", "OPTIONS"));
        config.setAllowedHeaders(List.of("*"));
        config.setAllowCredentials(false); // true would conflict with origin pattern "*"

        UrlBasedCorsConfigurationSource source = new UrlBasedCorsConfigurationSource();
        source.registerCorsConfiguration("/**", config);
        return source;
    }
}
